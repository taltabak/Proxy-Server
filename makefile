all:main.c
	gcc -Wall 203664487_threadpool.c 203664487_proxyServer.c -o ex3 -lpthread
all-GDB:main.c
	gcc -g -Wall 203664487_threadpool.c -g 203664487_proxyServer.c -g -o ex3 -lpthread