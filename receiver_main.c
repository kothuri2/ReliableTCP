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

#define RWS 1
#define MAX_SEQ_NO 16
#define FRAME_SIZE 1472
#define DATA_SIZE (FRAME_SIZE - sizeof(int))

typedef struct Frame {
	int sequence_num;
	char* data;
} frame;

int globalSocketUDP;
char fromAddr[100];
struct sockaddr_in theirAddr;
socklen_t theirAddrLen;
struct sockaddr_in ACKsender;
int NFE = 0;
int LFR = -1;
int request_number = 0;
int bytesRecvd;

void reliablyReceive(unsigned short int myUDPport, char* destinationFile) {

	printf("%s\n", "Waiting for sender...");

	unsigned char recData [FRAME_SIZE];
	FILE * fd = fopen(destinationFile, "w");
	while ((bytesRecvd = recvfrom(globalSocketUDP, recData, DATA_SIZE, 0, (struct sockaddr*)&theirAddr, &theirAddrLen)) > 0) {
		frame * newFrame = malloc(FRAME_SIZE);
		newFrame->sequence_num = *((int *)(recData));
		newFrame->data = ((char*)(recData + sizeof(int)));
		printf("%d %s\n", newFrame->sequence_num, newFrame->data);
		if(newFrame->sequence_num == request_number) {
			fwrite(newFrame->data, 1, strlen(newFrame->data), fd);
			fflush(fd);
			request_number = request_number + 1;
		}
		inet_ntop(AF_INET, &theirAddr.sin_addr, fromAddr, 100);
		sendto(globalSocketUDP, &request_number, sizeof(int), 0, (struct sockaddr*)&ACKsender, sizeof(ACKsender));
		free(newFrame);
	}

}

void setUpPortInfo(unsigned short int my_port) {
	//socket() and bind() our socket. We will do all sendto()ing and recvfrom()ing on this one.
	if((globalSocketUDP=socket(AF_INET, SOCK_DGRAM, 0)) < 0)
	{
		perror("socket");
		exit(1);
	}
	char myAddr[100];
	struct sockaddr_in bindAddr;
	sprintf(myAddr, "127.0.0.1");
	memset(&bindAddr, 0, sizeof(bindAddr));
	bindAddr.sin_family = AF_INET;
	bindAddr.sin_port = htons(my_port);
	inet_pton(AF_INET, myAddr, &bindAddr.sin_addr);
	if(bind(globalSocketUDP, (struct sockaddr*)&bindAddr, sizeof(struct sockaddr_in)) < 0)
	{
		perror("bind");
		close(globalSocketUDP);
		exit(1);
	}
	theirAddrLen = sizeof(theirAddr);

	memset(&ACKsender, 0, sizeof(ACKsender));
	ACKsender.sin_family = AF_INET;
	ACKsender.sin_port = htons(my_port);
	//inet_pton(AF_INET, receiver_hostname, &ACKsender.sin_addr);
}

int main(int argc, char** argv)
{
	unsigned short int udpPort;

	if(argc != 3)
	{
		fprintf(stderr, "usage: %s UDP_port filename_to_write\n\n", argv[0]);
		exit(1);
	}

	udpPort = (unsigned short int)atoi(argv[1]);
	setUpPortInfo(udpPort);
	reliablyReceive(udpPort, argv[2]);
}
