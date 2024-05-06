CFLAGS = -Wall -g -o0

all: server subscriber

common.o : common.cpp

server: server.cpp common.o

subscriber: subscriber.cpp common.o

clean:
	rm -f server subscriber