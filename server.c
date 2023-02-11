#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <errno.h>
#include <dirent.h>
#include <time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <math.h>
#include <pthread.h>
#include "biblioteca.h"


#define LIST_COMMAND 1
#define STATUS_COMMAND 2
#define DOWNLOAD_FILE 3
#define MAX_BYTES_SEND 500

//pasta e arquivo
DIR * folder;
char folderName[999];
FILE * actual;



//status do servidor
clock_t t;
struct tm * data_hora_ini;
time_t data_em_seg;
//int files_sent=0;
int numOfCon = 0;
long int bytes_downloaded=0;

//estrutura para guardar tamanho do arquivo de download
struct stat sb;

//função que lê o comando recebido
int readCommand(int socket){
  return recvInt(socket);
}

void statusCommand(int consocket){
  char * statusMessage = calloc(300, sizeof(char));
  char * line = calloc(60, sizeof(char));
  strcpy(statusMessage, "O estado atual do servidor é:\n");
  sprintf(line, "Servidor iniciado em: %d/%d/%d às %d:%d:%d\n", data_hora_ini->tm_mday, data_hora_ini->tm_mon+1, data_hora_ini->tm_year+1900, data_hora_ini->tm_hour, data_hora_ini->tm_min, data_hora_ini->tm_sec);
  strcat(statusMessage, line);
  sprintf(line, "%d conexões ao servidor\n", numOfCon);
  strcat(statusMessage, line);
  sprintf(line, "Numero de bytes enviados no momento = %lu", bytes_downloaded);
  strcat(statusMessage, line);
  sendString(statusMessage, consocket);

  free(statusMessage);
  free(line);
}

//listar arquivos do diretório do servidor
void listFiles(int consocket){
  rewinddir(folder);
  struct dirent * entry;
  int file_count = 0;
  while ((entry = readdir(folder)) != NULL) {
    if(entry->d_type == DT_REG) {
      file_count++;
    }
  }
  //envia o numero de arquivos
  sendInt(file_count, consocket);

  //rebobina a pasta
  rewinddir(folder);

  //envia os nomes dos arquivos
  while ((entry = readdir(folder)) != NULL) {
    if(entry->d_type == DT_REG) {
      sendString(entry->d_name, consocket);
    }
  }
  return;
}

//função de transferencia de arquivo para o cliente
void downloadFile(char * nomeArq, int wichPart, int howMany, int consocket){
  //printf("Solicitado: %s\n", nomeArq);

  //Cria o ponteiro pro arquivo solicitado (concatena o nome da pasta com /
  // e com o proprio nome do arquivo;
  char complete[999];
  char localFolderName[999];
  strcpy(localFolderName, folderName);
  strcpy(complete, strcat(localFolderName, nomeArq));

  FILE * arq2send = fopen(complete, "r");
  if(!arq2send){
    sendInt(-1, consocket);
    return;
  }

  sendInt(1, consocket);
  

  //calcular tamanho do arquivo
  if(stat(complete, &sb)){
    printf("errno: %d\n", errno);
  }
  double partSize = sb.st_size/(1.00*howMany);
  if(wichPart==0){
    bytes_downloaded+= sb.st_size;
  }

  int offsetStart = ceil(partSize/MAX_BYTES_SEND)* MAX_BYTES_SEND;
  if(offsetStart * (wichPart) > sb.st_size){
    sendInt(-1,consocket);
    printf("Thread Inválida: arquivo não pode ser particionado à partir desse intervalo\n");
    //close(consocket);
    return;
  }

  //envio de 500 em 500 bytes, ou seja, 'for' de 0 até a divisão do tamanho
  //do arquivo por 500 (ceil)
  int parts = ceil(partSize/(MAX_BYTES_SEND + 0.00));
  sendInt(parts, consocket);
  void * bytes2send = calloc(MAX_BYTES_SEND, 1);
  int fresult = fseek(arq2send, offsetStart*(wichPart), SEEK_SET);

  for(int i=0; i<parts; i++ ){
    int xBytes= 0;

    xBytes = fread(bytes2send, 1, MAX_BYTES_SEND, arq2send);

    sendInt(xBytes, consocket);

    if(xBytes==0){
      continue;
    }

    //envia os bytes
    sendVoid(bytes2send, xBytes, consocket);
  }
  printf("fim parte %d -------------------------------------------------\n", wichPart);
  return;
}


void* recvCommands(void * arguments){
  int consocket = *((int *) arguments);
      //char* res = recvString(consocket);
      printf("consocket: %d\n", consocket);

      int command = readCommand(consocket);
      //printf("command = %d\n", command);

      switch (command) {

        case LIST_COMMAND:
          t = clock();
          //printf("clock = %lf\n", ((double)t)/(CLOCKS_PER_SEC/1000));
          listFiles(consocket);
        break;

        case STATUS_COMMAND:
          statusCommand(consocket);
        break;

        case DOWNLOAD_FILE:
          printf("download\n");
          char* nomeArq = recvString(consocket);
          int wichPart, howMany;
          wichPart = recvInt(consocket);
          howMany = recvInt(consocket);
          //printf("%s\n", nomeArq);
          downloadFile(nomeArq, wichPart, howMany, consocket);
        break;
      }
      
      close(consocket);
      return NULL;
}

int main(int argc, char *argv[])
{
  time(&data_em_seg);
  data_hora_ini = localtime(&data_em_seg);
  printf("Servidor iniciado em: %d/%d/%d às %d:%d:%d\n", data_hora_ini->tm_mday, data_hora_ini->tm_mon+1, data_hora_ini->tm_year+1900, data_hora_ini->tm_hour, data_hora_ini->tm_min, data_hora_ini->tm_sec);
    if( argc != 3 ){

        printf("USO: diretorio num_porta\n");

        return EXIT_FAILURE;

    }

    //checagem de existência da pasta
    folder = opendir(argv[1]);
    if(!folder){
      printf("Pasta fornecida não existe!\n");
      return EXIT_FAILURE;
    }
    struct dirent * entry;
    int file_count = 0;
    //contagem de arquivos da pasta
    while ((entry = readdir(folder)) != NULL) {
      if(entry->d_type == DT_REG) {
        file_count++;
      }
    }
    printf("Servidor tem %d arquivos\n", file_count);

    strcpy(folderName, argv[1]);


    //limite de conexões
    pthread_t threads[ 99999 ];

    srand( time( NULL ));

    struct sockaddr_in dest; /* socket info about the machine connecting to us */
    struct sockaddr_in serv; /* socket info about our server */
    int mysocket;            /* socket used to listen for incoming connections */
    socklen_t socksize = sizeof(struct sockaddr_in);

    memset(&serv, 0, sizeof(serv));           /* zero the struct before filling the fields */
    serv.sin_family = AF_INET;                /* set the type of connection to TCP/IP */
    serv.sin_addr.s_addr = htonl(INADDR_ANY); /* set our address to any interface */
    serv.sin_port = htons( atoi( argv[ 2 ] ) );           /* set the server port number */

    mysocket = socket(AF_INET, SOCK_STREAM, 0);

    /* bind serv information to mysocket */
    bind(mysocket, (struct sockaddr *)&serv, sizeof(struct sockaddr));

    /* start listening, allowing a queue of up to 1 pending connection */
    listen(mysocket, 1);


    printf("Esperando conexões na porta:%s\n", argv[ 2 ] );
    int counter = 0;
    int consocket[99999];
    while(1){

      consocket[counter] = accept(mysocket, (struct sockaddr *)&dest, &socksize);
      printf("Conexão recebida de %s - esperando comandos...\n", inet_ntoa(dest.sin_addr));
      pthread_t thread;
      pthread_create( &threads[ counter ] , NULL, recvCommands, &consocket[counter]);
      counter++;
      numOfCon++;
    }

    closedir(folder);

    return EXIT_SUCCESS;
}
