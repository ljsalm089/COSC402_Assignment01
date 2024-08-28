CFLAGS=-g
ALL_HEADERS = ./net_util.h \
			./entity/client_info.h \
			./io/common_io.h \
			./io/buffer.h \
			./common.h \
			./collections/list.h \
			./entity/message.h

objects = ./entity/client_info.o \
			./io/buffer.o \
			./io/cache.o \
			./net_util.o \
			./entity/message.o

$(objects): %.o: %.c $(ALL_HEADERS)
	$(CC) -c $(CFLAGS) $< -o $@

server: IMServer.c $(ALL_HEADERS) $(objects)
	$(CC) -c $(CFLAGS) $(ALL_HEADERS) IMServer.c
	$(CC) $(objects) IMServer.o -o IMServer

client: IMClient.c $(ALL_HEADERS) $(objects)
	$(CC) -c $(CFLAGS) $(ALL_HEADERS) IMClient.c
	$(CC) $(objects) IMClient.o -o IMClient

main:
	$(CC) $(CFLAGS) main.c common.h -o main

all: server main client

clean:
	find . -name '*.o' | xargs rm
	find . -name '*.h.gch' | xargs rm
	rm -f main IMServer IMClient
