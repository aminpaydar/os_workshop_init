co_example:
	mkdir -p out && gcc -o out/co_example co_example.c co.c ch.c -lpthread && out/co_example

ch_example:
	mkdir -p out && gcc -o out/ch_example ch_example.c co.c ch.c -lpthread && out/ch_example