#include "biblioteca.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

void sendInt( int number, int socket );
int recvInt( int socket );

void sendDouble( double number, int socket );
double recvDouble( int socket );

void sendString( char* string, int socket );
char * recvString( int socket );

void sendVoid (void* buffer, int nBytes, int socket);
void * recvVoid (int socket);

void sendInt( int number, int socket){
  int bytesSent = 0;

  while((bytesSent+= send(socket, &number + bytesSent, sizeof(int) - bytesSent, 0))!= sizeof(int)){
    printf("send int loop %d\n", bytesSent);
    if(bytesSent==-1){
      printf("CLIENT ERROR: %s\nsocket %d", strerror(errno), socket);
    }
  }

  return;
}

int recvInt( int socket ){
  int number;
  int bytesRead = 0;
  while((bytesRead+= recv(socket, &number + bytesRead, sizeof(int) - bytesRead, 0))!= sizeof(int)){
    printf("recv int loop %d\n", bytesRead);
    if(bytesRead==-1){
      printf("CLIENT ERROR: %s\nsocket %d", strerror(errno), socket);
    }
  }

  return number;
}

void sendDouble( double number, int socket){
  send(socket, &number, sizeof(double), 0);
  return;
}

double recvDouble(int socket){
  double number;
  recv(socket, &number, sizeof(double), 0);
  return number;
}

void sendString( char* string, int socket){
  int len = strlen(string)* sizeof(char);
  sendInt( len, socket);
  int bytesSent = 0;
  while((bytesSent+= send(socket, string+ bytesSent, len- bytesSent, 0)) != len){
    printf("send string loop %d\n", bytesSent);
    if(bytesSent==-1){
      printf("ERROR: %s\n", strerror(errno));
    }
  }

  return;
}

char * recvString( int socket){
  int len = recvInt(socket);
  char * string = (char *) calloc(len, sizeof(char));
  int bytesRead = 0;
  while((bytesRead+= recv(socket, string+ bytesRead, len- bytesRead, 0)) != len){
    printf("recv string loop %d\n", bytesRead);
    if(bytesRead==-1){
      printf("ERROR: %s\n", strerror(errno));
    }
  }
  //null terminate
  string[len] = '\0';
  return string;
}

void sendVoid (void* buffer, int nBytes, int socket){
  sendInt(nBytes, socket);
  int bytesSent = 0;
  while((bytesSent+= send(socket, buffer + bytesSent, nBytes - bytesSent, 0))!= nBytes){
    printf("loop %d\n", bytesSent);
    if(bytesSent==-1){
      printf("Server ERROR: %s\n", strerror(errno));
    }
  }
}

void * recvVoid ( int socket ){
  int nBytes = recvInt(socket);
  void * buffer = (void *) calloc (nBytes, sizeof(void));
  int bytesRead = 0;
  while((bytesRead+= recv(socket, buffer + bytesRead, nBytes - bytesRead, 0))!= nBytes){
    printf("loop %d\n", bytesRead);
    if(bytesRead==-1){
      printf("CLIENT ERROR: %s\n", strerror(errno));
    }
  }
  return buffer;
}
