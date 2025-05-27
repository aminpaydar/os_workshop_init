co_example:
	gcc -o co_example co_example.c co.c -pthread && ./co_example 

ch_example:
	gcc -o ch_example ch_example.c co.c ch.c -pthread && ./ch_example 