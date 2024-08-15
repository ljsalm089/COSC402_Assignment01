server:
	gcc IMServer.c common.h collections/list.h io/cache.c io/common_io.h -o IMServer

client:
	gcc IMClient.c common.h collections/list.h -o IMClient

main:
	gcc main.c common.h -o main

all: server main client

clean:
	rm main IMServer IMClient