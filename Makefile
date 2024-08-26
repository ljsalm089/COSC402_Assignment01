ALL_SRC = common.h collections/list.h io/cache.c io/common_io.h \
		io/buffer.h io/buffer.c net_util.h net_util.c entity/client_info.h entity/client_info.c


server:
	gcc IMServer.c ${ALL_SRC} -o IMServer

client:
	gcc IMClient.c common.h collections/list.h -o IMClient

main:
	gcc main.c common.h -o main

all: server main client

clean:
	rm -f main IMServer IMClient
