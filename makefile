CC = gcc
CFLAGS = -pthread -g
#CFLAGS = -Wall -g -lpthread -Wextra -fsanitize=address -fsanitize=undefined,float-divide-by-zero -fsanitize-address-use-after-scope -lasan -lubsan

all: client server

client.o: client.c client.h
	$(CC) $(CFLAGS) -g client.c -c

server.o: server.c server.h
	$(CC) $(CFLAGS) server.c -c

common.o: common.c common.h
	$(CC) $(CFLAGS) common.c -c

linkedlist.o: linkedlist.c linkedlist.h
	$(CC) $(CFLAGS) linkedlist.c -c

client: client.o common.o
	$(CC) $(CFLAGS) -g client.o common.o -o client

server: server.o common.o linkedlist.o
	$(CC) $(CFLAGS) server.o common.o linkedlist.o -o server

clean:
	rm client server client.o server.o common.o linkedlist.o
