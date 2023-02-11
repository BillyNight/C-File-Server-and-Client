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
#include <math.h>
#include <fcntl.h>
#include <pthread.h>
#include "biblioteca.h"

#define MAX_FILENAME 99
#define MAX_NUMPARTS 10
#define MAX_BYTES2READ 500

#define MAXRCVLEN 500
#define LIST_COMMAND 1
#define STATUS_COMMAND 2
#define DOWNLOAD_FILE 3
#define MAX_BYTES_RECV 500

//Nomes dos arquivos provenientes de argv
char fileNames[990];
char fileNamesList[10][99];

//Vetor que armazena os sockets para cada parte de cada arquivo
int fileSockets[10][60];

//Estrutura para guardar as propriedades de cada arquivo e cada parte de arquivo
struct fileProps {
	char fileName[99];
	int wichPart;
	int howMany;
	int socket;
	int filePos;
};

//Salva caminho da pasta de download
char baseFileFolder[99];

int port;
int command;

int establishConnection (){
	//CÓDIGO PARA CONEXÃO ----------------------------------------------------------------------------------------
	char buffer[MAXRCVLEN + 1]; /* +1 so we can add null terminator */
	bzero( buffer, MAXRCVLEN + 1 );
	int len, mysocket;
	struct sockaddr_in dest;

	mysocket = socket(AF_INET, SOCK_STREAM, 0);

	memset(&dest, 0, sizeof(dest));                /* zero the struct */
	dest.sin_family = AF_INET;
	dest.sin_addr.s_addr = htonl(INADDR_LOOPBACK); /* set destination IP number - localhost, 127.0.0.1*/
	dest.sin_port = htons( port );                /* set destination port number */

	int connectResult = connect(mysocket, (struct sockaddr *)&dest, sizeof(struct sockaddr_in));
	if( connectResult == - 1 ){

		 printf("CLIENT ERROR: %s\n", strerror(errno));
		 return -1;
	}
	printf("mysocket = %d\n", mysocket);
	return mysocket;
	//FIM DO CÓDIGO PARA CONEXÃO ----------------------------------------------------------------------------------
}


void listFiles(){
	int mysocket = establishConnection();
	if(mysocket==-1){
		return;
	}

	sendInt(command, mysocket);

	//numero de arquivos
	int k = recvInt(mysocket);
	printf("Existem %d arquivos no servidor\n", k);
	printf("Lista dos nomes:\n");
	for(int i=0; i<k; i++){
			printf("%s\n", recvString(mysocket));
	}
	printf("\nFim da lista\n");
}

void statusCommand(){
	int mysocket = establishConnection();
	if(mysocket==-1){
		return;
	}

	sendInt(command, mysocket);
	printf("%s\n", recvString(mysocket));

}

void * downloadFile(void * crudeProps){
	//printf("Entered download\n");
	int * threadResult = calloc(1, sizeof(int));
	struct fileProps * props = (struct fileProps *) crudeProps;
	char * fileName = props->fileName;
	int wichPart= props->wichPart;
	int howMany= props->howMany;
	int mysocket = props->socket;

	
	//Seleciona a opção download
	sendInt(3, mysocket);
	//printf("sent command for thread %d\n", wichPart);

	//Requisição ao servidor
	sendString(fileName, mysocket);

	//qual a thread
	sendInt(wichPart, mysocket);


	//quantidade de threads
	sendInt(howMany, mysocket);





	int res = recvInt(mysocket);
	
	if(res == -1){
		printf("Arquivo inexistente\n");
		//close(mysocket);
		*threadResult = -1;
		//printf("retornando\n");
		return (void*) threadResult;
	}



	//Recebe quantas partes de 500 bytes o arquivo será dividido
	int parts = recvInt(mysocket);
	if(parts==-1){
		printf("Thread inválida: o arquivo não pode ser particionado nesta parte %d de %d\n", wichPart, howMany);
		*threadResult = 0;
		//close(mysocket);
		return (void*) threadResult;
	}

	//nome do arquivo concatena com o numero da parte do arquivo
	char * filePartName = (char *) calloc(strlen(fileName)+strlen(baseFileFolder)+4, sizeof(char));

	char* num = (char*) calloc(MAX_NUMPARTS, sizeof(char));

	sprintf(num, "%d", wichPart+1);
	strcpy(filePartName, baseFileFolder);
	strcat(filePartName, fileName);
	strcat(filePartName, num);

	FILE * filePart = fopen(filePartName, "w+");

	void * bytes2write = calloc(MAX_BYTES_RECV,1);
	int xBytes=0;
	for(int i=0; i<parts; i++){
		//recebe quantos bytes serão recebidos (max 500)
		xBytes=recvInt(mysocket);
		if(xBytes==0){
			continue;
		}

		//recebe os bytes a serem escritos
		bytes2write = recvVoid(mysocket);

		fwrite(bytes2write, 1, xBytes, filePart);
	}
	fclose(filePart);

	free(filePartName);
	free(num);
	*threadResult = 1;
	return (void*) threadResult;
}


void * initDowloadAndCombine (void * crudeProps){
	//printf("Init download\n");
	int * threadResult = calloc(1, sizeof(int));

	struct fileProps * currentProps = (struct fileProps *) crudeProps;
	char * fileName = currentProps->fileName;
	int howMany = currentProps->howMany;
	int filePos = currentProps->filePos;

	pthread_t threads[ 60 ];

	struct fileProps props[50];

	for(int i=0; i< howMany; i++){

		props[i].socket = fileSockets[filePos][i];		
		strcpy(props[i].fileName, fileName);
		props[i].howMany = howMany;
		props[i].wichPart = i;
		pthread_create( &threads[ i ], NULL, downloadFile, &props[i]);
	}
	void* returnValue;
	int limit = 0;
	int * d;
	//Espera as partes voltarem
	for(int i=0; i< howMany; i++){

		pthread_join(threads[i], &returnValue);

		 d = (int*) returnValue;

		if(*d==-1){
			*threadResult = -1;
			return (void*) threadResult;
		}else if(*d==0 && limit==0){
			//Encerrou antes da thread alvo, fará o combine até a parte que voltou
			printf("Arquivo: %s Thread %d de %d inválida\n", fileName, i+1, howMany);
			limit=i;
		}else if(*d==1){
			printf("Arquivo: %s Etapa %d de %d concluída\n", fileName, i+1, howMany);
		}
		
	}

	if(limit==0){
		limit=howMany;
	}

	free(d);

	//Combinar arquivos de parts
	char* num = (char*) calloc(MAX_NUMPARTS, sizeof(char));
	char* buffer = (char*) calloc(MAX_BYTES_RECV, sizeof(char));
	char fileNameMod [99];
	strcpy(fileNameMod, baseFileFolder);
	strcat(fileNameMod, fileName);
	int file = open(fileNameMod, O_CREAT|O_WRONLY, 0666);
	//printf("file: %d\n", file);

	char localFileName[99];
  	for(int i=1; i<=limit; i++){
		strcpy(localFileName, baseFileFolder);
		strcat(localFileName, fileName);
		sprintf(num, "%d", i);
		strcat(localFileName, num);
		printf("%s\n", localFileName);

		int filePart = open(localFileName, O_RDONLY);
		//printf("filePart: %d\n", filePart);

		while(1){
			int numBytesRead = read(filePart, buffer, MAX_BYTES_RECV);
			int numBytesWrote = write(file, buffer, numBytesRead);
			if(numBytesRead<MAX_BYTES_RECV){
				break;
			}
		}
		close(filePart);
		remove(localFileName);
		memset(localFileName, '\0', MAX_FILENAME);
		memset(num, '\0', MAX_NUMPARTS);
  	}
	free(num);
	free(buffer);
	*threadResult = 1;
	return (void*) threadResult;

}

int main(int argc, char *argv[])
{


	if( argc < 4 ){
		printf("Bem-vindo ao cliente 1.0\n");
		printf("A lista de comandos é:\n");
		printf("./client PASTA_DOWNLOADS NUM_PORTA 1  \n------ Lista os arquivos do servidor\n\n");
		printf("./client PASTA_DOWNLOADS NUM_PORTA 2  \n------ Exibe o estado atual do servidor\n\n");
		printf("./client PASTA_DOWNLOADS NUM_PORTA 3 nome_arq1,nome_arq2... NUM_THREADS \n------ Faz download dos arquivos específicados e em quantas threads\n");

        return EXIT_FAILURE;

    }
	


		strcpy(baseFileFolder, argv[1]);
		printf("Download Folder path: %s\n", baseFileFolder);
		DIR * folder;
		folder = opendir(argv[1]);
		if(!folder){
			printf("Diretorio de downloads não existe...\n");
			return EXIT_FAILURE;
		}

		port = atoi( argv[ 2 ]);
		printf("port %d\n", port);

		command = atoi(argv[3]);

		if(command > 3 || command < 1){
			printf("ERRO: Comando inexistente\n");
			return 0;
		}

		switch (command) {

		 	case LIST_COMMAND:
				listFiles();
			break;

			case STATUS_COMMAND:
				statusCommand();
			break;

			case DOWNLOAD_FILE:
				if(argc < 6){
					printf("Insira o nome dos arquivos, separados por vírgula.\nDepois insira a quantidade de threads (1-50)\n");
					break;
				}
				if(atoi(argv[5])<=0 || atoi(argv[5])>50){
					printf("Quantidade de threads inválida, só aceita-se valores entre 1 e 50\n");
					break;
				}
				struct fileProps currentProps[10];
				pthread_t fileThreads[ 10 ];
				strcpy(fileNames, argv[4]);
				char *pt;
				int counter=0;
				pt=strtok(fileNames, ",");
				while(pt){
					strcpy(fileNamesList[counter], pt);
					pt=strtok(NULL, ",");
					counter++;
				}
				int howManyThreads = atoi(argv[5]);

				//Faz alocação de todos os sockets
				for(int i=0; i<counter; i++){
					for(int j=0; j< howManyThreads; j++){
						fileSockets[i][j] = establishConnection();
					}
				}

				//Cria cada thread "mãe" para cada arquivo e envia suas propriedades
				for(int i=0; i<counter; i++){
					strcpy(currentProps[i].fileName, fileNamesList[i]);
					currentProps[i].howMany = atoi(argv[5]);
					currentProps[i].filePos = i;

					printf("arq %d = %s \n", i, currentProps[i].fileName);
					pthread_create( &fileThreads[ i ], NULL, initDowloadAndCombine, &currentProps[i]);
				}
				void* returnValue = NULL;
				for(int i=0; i<counter; i++){
					pthread_join(fileThreads[i], returnValue);
					printf("Arquivo na pos %d retornou\n", i);
				}

			break;
		 }

   return EXIT_SUCCESS;
}
