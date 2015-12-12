all: server client

CC=gcc
CFLAGS=-I.
DEPS = # header file z
OBJ = server.o
OBJ2 = client.o

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

server: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)

client: $(OBJ2)
	$(CC) -o $@ $^ $(CFLAGS)

clean:
	rm -f server.o client.o server client
