CC=g++

CFLAGS=-Wall -W -g  

all: client server

client: duckchat_client.c 
	gcc duckchat_client.c $(CFLAGS) -std=c11 -o client -pthread

server: duckchat_server.cpp 
	g++ $< $(CFLAGS) -std=c++11 -o server 

clean:
	rm -f client server *.o

