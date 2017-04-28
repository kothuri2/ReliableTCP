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
	int sequence_num;
	char* data;
} frame;

int sockfd; /* socket */
int clientlen; /* byte size of client's address */
struct sockaddr_in serveraddr; /* server's addr */
struct sockaddr_in clientaddr; /* client addr */
struct hostent *hostp; /* client host info */
char *hostaddrp; /* dotted decimal host addr string */
int optval; /* flag value for setsockopt */
socklen_t sendersize;
int request_number = 0;
int maxposs;
int bytesRecvd;
unsigned long long int bytesToWrite;

void reliablyReceive(unsigned short int myUDPport, char* destinationFile) {

	printf("%s\n", "Waiting for sender...");

	unsigned char recData [FRAME_SIZE];
	unsigned char recDataBuffer [WINDOW_SIZE * FRAME_SIZE];
	FILE * fd = fopen(destinationFile, "w");
	while (1)
	{
		int i = 0;
		for(; i < WINDOW_SIZE; i++) {
			if(bytesToWrite == 0) {
				fclose(fd);
				printf("%s\n", "Successfully Received File");
				return;
			}
			bytesRecvd = recvfrom(sockfd, recData, FRAME_SIZE, 0, (struct sockaddr*)&clientaddr, &clientlen);
			frame * newFrame = malloc(FRAME_SIZE);
			newFrame->sequence_num = *((int *)(recData));
			int datasize = 0;
			if(newFrame->sequence_num == 0) {
				bytesToWrite = *((unsigned long long int*)(recData+sizeof(int)));
				bytesToWrite = ((int)bytesToWrite) - (bytesRecvd-(int)sizeof(int)-(int)sizeof(unsigned long long int));
				newFrame->data = (char*)(recData+sizeof(int)+sizeof(unsigned long long int));
				datasize = (bytesRecvd-(int)sizeof(int)-(int)sizeof(unsigned long long int));
			} else {
				newFrame->data = (char*)(recData+sizeof(int));
				bytesToWrite = ((int)bytesToWrite) - (bytesRecvd-(int)sizeof(int));
				datasize = (bytesRecvd-(int)sizeof(int));
			}
			int order = (newFrame->sequence_num) % (WINDOW_SIZE);
			memcpy(recDataBuffer + (order * WINDOW_SIZE), newFrame->data, datasize);
			printf("Server received packet %d\n", newFrame->sequence_num);
			if(newFrame->sequence_num == request_number) {
				request_number++;
			}
			free(newFrame);
			hostp = gethostbyaddr((const char *)&clientaddr.sin_addr.s_addr, sizeof(clientaddr.sin_addr.s_addr), AF_INET);
	    	hostaddrp = inet_ntoa(clientaddr.sin_addr);
			sendto(sockfd, ((const void *) &request_number), sizeof(int), 0, (struct sockaddr*)&clientaddr, clientlen);
		}
		fwrite(recDataBuffer, 1, WINDOW_SIZE*FRAME_SIZE, fd);
		fflush(fd);
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
	maxposs = 2*WINDOW_SIZE;

	if(argc != 3)
	{
		fprintf(stderr, "usage: %s UDP_port filename_to_write\n\n", argv[0]);
		exit(1);
	}

	udpPort = (unsigned short int)atoi(argv[1]);
	setUpPortInfo(udpPort);
	reliablyReceive(udpPort, argv[2]);
}
