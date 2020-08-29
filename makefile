all:main.c
	gcc -Wall threadpool.c proxyServer.c -o proxy -lpthread
all-GDB:main.c
	gcc -g -Wall threadpool.c -g proxyServer.c -g -o proxy -lpthread
