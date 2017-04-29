#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>

#define FRAME_SIZE 1472
#define DATA_SIZE (FRAME_SIZE - sizeof(int))
#define WINDOW_SIZE 4

typedef struct Frame {
	unsigned long sequence_num;
	char* data;
} frame;

typedef struct Data {
	char* buffer;
	int size;
} data;

int sockfd; /* socket */
int clientlen; /* byte size of client's address */
struct sockaddr_in serveraddr; /* server's addr */
struct sockaddr_in clientaddr; /* client addr */
struct hostent *hostp; /* client host info */
char *hostaddrp; /* dotted decimal host addr string */
int optval; /* flag value for setsockopt */
socklen_t sendersize;
unsigned long request_number;
int numneeded;
int bytesRecvd;
unsigned long long int bytesToWrite = -1;

void reliablyReceive(unsigned short int myUDPport, char* destinationFile) {

	printf("%s\n", "Waiting for sender...");

	//unsigned char recData [FRAME_SIZE];
	//unsigned char recDataBuffer [WINDOW_SIZE * FRAME_SIZE];
	FILE * fd = fopen(destinationFile, "w");
	while(1)
	{
		data* recDataBuffer = malloc(WINDOW_SIZE * sizeof(data));
		int i;
		for(i = 0; i < WINDOW_SIZE; i++)
			recDataBuffer[i].buffer = NULL;
		numneeded = WINDOW_SIZE;
		while(1) {
			if(bytesToWrite == 0) {
				for(i = 0; i < WINDOW_SIZE; i++) {
					if(recDataBuffer[i].buffer != NULL) {
						fwrite(recDataBuffer[i].buffer, 1, recDataBuffer[i].size, fd);
						fflush(fd);
						free(recDataBuffer[i].buffer);
					}
				}
				free(recDataBuffer);
				fclose(fd);
				printf("%s\n", "Successfully Received File");
				return;
			}

			if(numneeded == 0)
				break;
			unsigned char recData[FRAME_SIZE];
			bytesRecvd = recvfrom(sockfd, recData, FRAME_SIZE, 0, (struct sockaddr*)&clientaddr, &clientlen);
			frame * newFrame = malloc(FRAME_SIZE);
			newFrame->sequence_num = *((unsigned long *)(recData));
			int order = (newFrame->sequence_num)%(WINDOW_SIZE);
			if(newFrame->sequence_num < request_number || newFrame->sequence_num >= request_number + WINDOW_SIZE || recDataBuffer[order].buffer != NULL)
				continue;
			numneeded--;
			int datasize = 0;
			if(newFrame->sequence_num == 0) {
				bytesToWrite = *((unsigned long long int*)(recData+sizeof(unsigned long)));
				datasize = (bytesRecvd-sizeof(unsigned long)-sizeof(unsigned long long int));
				bytesToWrite = bytesToWrite - datasize;
				newFrame->data = (char*)(recData+sizeof(unsigned long)+sizeof(unsigned long long int));
			} else {
				newFrame->data = (char*)(recData+sizeof(unsigned long));
				datasize = (bytesRecvd-sizeof(unsigned long));
				bytesToWrite = bytesToWrite - datasize;
			}
			recDataBuffer[order].buffer = strdup(newFrame->data);
			recDataBuffer[order].size = datasize;
			//printf("bytesToWrite: %llu bytesRecvd: %d sizeof int: %lu, sizeof long: %lu\n", bytesToWrite, bytesRecvd, sizeof(int), sizeof(unsigned long long int));
			printf("Server received packet %lu\n", newFrame->sequence_num);
			printf("order: %d\n", order);
			if(newFrame->sequence_num == request_number)
				request_number++;
			free(newFrame);
			hostp = gethostbyaddr((const char *)&clientaddr.sin_addr.s_addr, sizeof(clientaddr.sin_addr.s_addr), AF_INET);
	    hostaddrp = inet_ntoa(clientaddr.sin_addr);
			sendto(sockfd, ((const void *) &request_number), sizeof(unsigned long), 0, (struct sockaddr*)&clientaddr, clientlen);
		}
		for(i = 0; i < WINDOW_SIZE; i++) {
			fwrite(recDataBuffer[i].buffer, 1, recDataBuffer[i].size, fd);
			fflush(fd);
			free(recDataBuffer[i].buffer);
		}
		free(recDataBuffer);
		numneeded = WINDOW_SIZE;
	}

}

void setUpPortInfo(unsigned short int my_port) {
	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	optval = 1;
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval , sizeof(int));
	bzero((char *) &serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serveraddr.sin_port = htons((unsigned short)my_port);
	bind(sockfd, (struct sockaddr *) &serveraddr, sizeof(serveraddr));
	clientlen = sizeof(clientaddr);
}

int main(int argc, char** argv)
{
	unsigned short int udpPort;
	bytesToWrite = -1;
	request_number = 0;

	if(argc != 3)
	{
		fprintf(stderr, "usage: %s UDP_port filename_to_write\n\n", argv[0]);
		exit(1);
	}

	udpPort = (unsigned short int)atoi(argv[1]);
	setUpPortInfo(udpPort);
	reliablyReceive(udpPort, argv[2]);
}
