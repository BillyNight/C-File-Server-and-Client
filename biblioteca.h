
#ifndef BIBLIOTECA_H
#define BIBLIOTECA_H
//gcc -c biblioteca.c -o biblioteca.o
//ar rc libbiblioteca.a biblioteca.o

void sendInt( int number, int socket );
int recvInt( int socket );

void sendDouble( double number, int socket );
double recvDouble( int socket );

void sendString( char* string, int socket );
char * recvString( int socket );

void sendVoid (void* buffer, int nBytes, int socket);
void * recvVoid ( int socket);

#endif
