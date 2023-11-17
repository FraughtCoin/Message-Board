CFLAGS = -Wall

all: server subscriber

server:
	gcc server.c -o server $(CFLAGS)

subscriber:
	gcc subscriber.c -o subscriber $(CFLAGS)

clean:
	rm -rf server subscriber