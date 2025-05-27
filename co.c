#include "co.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdatomic.h>
#include <signal.h>
#include <string.h>
#ifdef __linux__
#include <sys/prctl.h>
#else
#include <signal.h>
#endif

// ساختار داخلی برای مدیریت سیستم همکاری
typedef struct {
    pthread_t pool[WORKER_COUNT];      // آرایه نخ‌های کارگر
    atomic_int active_workers;         // تعداد کارگران فعال
    atomic_bool system_active;         // وضعیت فعال/غیرفعال سیستم
    
    struct {
        task_t items[TASK_QUEUE_SIZE]; // صف وظایف
        atomic_int front;              // اشاره‌گر به ابتدای صف
        atomic_int rear;               // اشاره‌گر به انتهای صف
        pthread_mutex_t lock;          // قفل برای دسترسی به صف
        pthread_cond_t not_empty;      // شرط برای صف غیرخالی
        pthread_cond_t not_full;       // شرط برای صف غیرپر
    } task_queue;
} co_system_t;

static co_system_t co_sys;

// تابع کمکی برای بررسی پر بودن صف
static int is_queue_full() {
    return ((co_sys.task_queue.rear + 1) % TASK_QUEUE_SIZE) == co_sys.task_queue.front;
}

// تابع کمکی برای بررسی خالی بودن صف
static int is_queue_empty() {
    return co_sys.task_queue.front == co_sys.task_queue.rear;
}

// تابع اجرایی هر کارگر
static void* worker_routine(void* arg) {
    int worker_id = (int)(long)arg;
    char thread_name[16];
    snprintf(thread_name, sizeof(thread_name), "co-worker-%d", worker_id);
    
    #ifdef __linux__
    prctl(PR_SET_NAME, thread_name, 0, 0, 0);
    #endif
    
    while (atomic_load(&co_sys.system_active)) {
        pthread_mutex_lock(&co_sys.task_queue.lock);
        
        // انتظار برای وجود وظیفه در صف
        while (is_queue_empty() && atomic_load(&co_sys.system_active)) {
            pthread_cond_wait(&co_sys.task_queue.not_empty, &co_sys.task_queue.lock);
        }
        
        if (!atomic_load(&co_sys.system_active)) {
            pthread_mutex_unlock(&co_sys.task_queue.lock);
            break;
        }
        
        // دریافت وظیفه از صف
        task_t task = co_sys.task_queue.items[co_sys.task_queue.front];
        co_sys.task_queue.front = (co_sys.task_queue.front + 1) % TASK_QUEUE_SIZE;
        
        // اطلاع به تولیدکنندگان در صورت نیاز
        pthread_cond_signal(&co_sys.task_queue.not_full);
        pthread_mutex_unlock(&co_sys.task_queue.lock);
        
        // اجرای وظیفه
        if (task.func) {
            task.func(task.arg);
        }
    }
    
    atomic_fetch_sub(&co_sys.active_workers, 1);
    return NULL;
}

void co_init() {
    // مقداردهی اولیه صف وظایف
    atomic_init(&co_sys.task_queue.front, 0);
    atomic_init(&co_sys.task_queue.rear, 0);
    pthread_mutex_init(&co_sys.task_queue.lock, NULL);
    pthread_cond_init(&co_sys.task_queue.not_empty, NULL);
    pthread_cond_init(&co_sys.task_queue.not_full, NULL);
    
    // راه‌اندازی سیستم
    atomic_store(&co_sys.system_active, true);
    atomic_store(&co_sys.active_workers, WORKER_COUNT);
    
    // ایجاد نخ‌های کارگر
    for (int i = 0; i < WORKER_COUNT; i++) {
        pthread_create(&co_sys.pool[i], NULL, worker_routine, (void*)(long)(i+1));
    }
}

void co_shutdown() {
    // توقف سیستم
    atomic_store(&co_sys.system_active, false);
    pthread_cond_broadcast(&co_sys.task_queue.not_empty);
    
    // انتظار برای پایان کار کارگران
    while (atomic_load(&co_sys.active_workers) > 0) {
        usleep(1000); // کاهش مصرف CPU
    }
    
    // پاک‌سازی منابع
    pthread_mutex_destroy(&co_sys.task_queue.lock);
    pthread_cond_destroy(&co_sys.task_queue.not_empty);
    pthread_cond_destroy(&co_sys.task_queue.not_full);
}

void co(task_func_t func, void* arg) {
    pthread_mutex_lock(&co_sys.task_queue.lock);
    
    // انتظار برای فضای خالی در صف
    while (is_queue_full() && atomic_load(&co_sys.system_active)) {
        pthread_cond_wait(&co_sys.task_queue.not_full, &co_sys.task_queue.lock);
    }
    
    if (!atomic_load(&co_sys.system_active)) {
        pthread_mutex_unlock(&co_sys.task_queue.lock);
        return;
    }
    
    // اضافه کردن وظیفه به صف
    co_sys.task_queue.items[co_sys.task_queue.rear].func = func;
    co_sys.task_queue.items[co_sys.task_queue.rear].arg = arg;
    co_sys.task_queue.rear = (co_sys.task_queue.rear + 1) % TASK_QUEUE_SIZE;
    
    // اطلاع به کارگران
    pthread_cond_signal(&co_sys.task_queue.not_empty);
    pthread_mutex_unlock(&co_sys.task_queue.lock);
}

int wait_sig() {
    sigset_t mask;
    int signum;
    
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGTERM);
    
    pthread_sigmask(SIG_BLOCK, &mask, NULL);
    printf("System ready. Waiting for termination signal...\n");
    
    if (sigwait(&mask, &signum) != 0) {
        perror("sigwait failed");
        return -1;
    }
    
    printf("Received signal %d. Initiating shutdown...\n", signum);
    return signum;
}
