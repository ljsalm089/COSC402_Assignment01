ALL_HEADERS = ./net_util.h \
			./entity/client_info.h \
			./io/common_io.h \
			./io/buffer.h \
			./common.h \
			./collections/list.h

objects = ./entity/client_info.o \
			./io/buffer.o \
			./io/cache.o \
			./net_util.o \
			./detectend.o

$(objects): %.o: %.c $(ALL_HEADERS)
	$(CC) -c $(CFLAGS) $< -o $@

server: IMServer.c $(ALL_HEADERS) $(objects)
	$(CC) -c $(CFLAGS) $(ALL_HEADERS) IMServer.c
	$(CC) $(objects) IMServer.o -o IMServer

client:
	gcc IMClient.c common.h collections/list.h -o IMClient

main:
	gcc main.c common.h -o main

all: server main client

clean:
	rm -f main IMServer IMClient
