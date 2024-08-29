CC=/usr/bin/gcc
CFLAGS=-g -lpthread
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
	$(CC) -c $< $(CFLAGS) -o $@

server: IMServer.c $(ALL_HEADERS) $(objects)
	$(CC) -c $(ALL_HEADERS) IMServer.c $(CFLAGS)
	$(CC) $(objects) $(CFLAGS) IMServer.o -o IMServer

client: IMClient.c $(ALL_HEADERS) $(objects)
	$(CC) -c $(ALL_HEADERS) $(CFLAGS) IMClient.c
	$(CC) $(objects) $(CFLAGS) IMClient.o -o IMClient

all: server client

clean:
	find . -name '*.o' | xargs rm
	find . -name '*.h.gch' | xargs rm
	rm -f IMServer IMClient
