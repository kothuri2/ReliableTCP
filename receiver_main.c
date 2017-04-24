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

// typedef struct Frame {
// 	int* sequence_num;
// 	void* data;
// } frame;

int globalSocketUDP;
struct sockaddr_in theirAddr;
socklen_t theirAddrLen;
//int bitmap[MAX_SEQ_NO];
int NFE = 0;
int LFR = -1;
//unsigned char recvBuf [10];
int bytesRecvd;

void reliablyReceive(unsigned short int myUDPport, char* destinationFile) {

	printf("%s\n", "Waiting for sender...");

	//frame* newframe = malloc(sizeof(frame));
	int* sequence_num = malloc(sizeof(int));
	void* data = malloc(FRAME_SIZE);
	int fd = open(destinationFile, O_WRONLY | O_CREAT | O_TRUNC);

	while ((bytesRecvd = recvfrom(myUDPport, data, DATA_SIZE, 0, (struct sockaddr*)&theirAddr, &theirAddrLen)) > 0) {
		strncpy((void*)sequence_num, data, sizeof(int));
		void* filedata = data + sizeof(int);
		if(*sequence_num == NFE && bytesRecvd == FRAME_SIZE) {
			write(fd, filedata, DATA_SIZE);
			LFR = NFE;
			NFE = (NFE + 1) % MAX_SEQ_NO;
			sendto(myUDPport, &LFR, sizeof(int), 0, (struct sockaddr*)&theirAddr, theirAddrLen);
		}
		// else if(sequence_num == NFE && ) {
		//
		// }
		else{
			sendto(myUDPport, &LFR, sizeof(int), 0, (struct sockaddr*)&theirAddr, theirAddrLen);
		}

	}
	//printf("%s\n", recvBuf);

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
