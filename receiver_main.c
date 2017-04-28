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
int firstflag = 1;
int bytesRecvd;
unsigned long long int bytesToWrite;

void reliablyReceive(unsigned short int myUDPport, char* destinationFile) {

	printf("%s\n", "Waiting for sender...");

	unsigned char recData [FRAME_SIZE];
	unsigned char recWindowBuffer [(WINDOW_SIZE)*DATA_SIZE];
	FILE * fd = fopen(destinationFile, "w");
	while (1)
	{
		int i=0;
		for(; i<WINDOW_SIZE; i++)
		{
			if(bytesToWrite == 0) {
				fclose(fd);
				printf("%s\n", "Successfully Received File");
				return;
			}

			bytesRecvd = recvfrom(sockfd, recData, FRAME_SIZE, 0, (struct sockaddr*)&clientaddr, &clientlen);
			frame * newFrame = malloc(FRAME_SIZE);
			int datasize;
			newFrame->sequence_num = *((int *)(recData));
			if(firstflag) {
				bytesToWrite = *((unsigned long long int*)(recData+sizeof(int)));
				datasize = bytesRecvd-(int)sizeof(int)-(int)sizeof(unsigned long long int);
				bytesToWrite = ((int)bytesToWrite) - datasize;
				newFrame->data = (char*)(recData+sizeof(int)+sizeof(unsigned long long int));
				firstflag = 0;
			} else {
				newFrame->data = (char*)(recData + sizeof(int));
				datasize = bytesRecvd-(int)sizeof(int);
				bytesToWrite = ((int)bytesToWrite) - datasize;
			}
			// Packet came out of order
			// Don't discard just write to buffer in order
			//Just write to buffer in order
			int order;
			// 1, 7
			if(newFrame->sequence_num < request_number) {
				order = ((maxposs+1)-request_number) + newFrame->sequence_num;
			} else {
				order = ((newFrame->sequence_num) - (request_number));
				if(newFrame->sequence_num == request_number) {
					request_number = (request_number + 1)%(maxposs + 1);
				}
			}
			memcpy(recWindowBuffer + (order * DATA_SIZE), newFrame->data, datasize);
			printf("Server received packet %d\n", newFrame->sequence_num);
			free(newFrame);

			hostp = gethostbyaddr((const char *)&clientaddr.sin_addr.s_addr, sizeof(clientaddr.sin_addr.s_addr), AF_INET);
	    	hostaddrp = inet_ntoa(clientaddr.sin_addr);
			sendto(sockfd, ((const void *) &request_number), sizeof(int), 0, (struct sockaddr*)&clientaddr, clientlen);
		}
		fwrite(recWindowBuffer, 1, sizeof(recWindowBuffer), fd);
		fflush(fd);
		request_number =
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
