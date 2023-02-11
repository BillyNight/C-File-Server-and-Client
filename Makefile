all: server client

client: client.o
	gcc -o client client.o biblioteca.o -pthread

client.o: client.c biblioteca.c
	gcc -c -g client.c biblioteca.c

server: server.o
	gcc -o server server.o biblioteca.o -lm -pthread

server.o: server.c biblioteca.c
	gcc -c -g server.c biblioteca.c

biblioteca.o: biblioteca.c
	gcc -c biblioteca.c -o biblioteca.o

clean:
	rm *.o
