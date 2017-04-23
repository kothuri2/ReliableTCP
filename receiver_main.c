#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

int globalSocketUDP;
#define RWS = 8;
#define MAX_SEQ_NO = 16;
#define FRAME_SIZE = 1472;


void reliablyReceive(unsigned short int myUDPport, char* destinationFile) {
	struct sockaddr_in theirAddr;
	socklen_t theirAddrLen;
	unsigned char recvBuf [10];
	int bytesRecvd;
	theirAddrLen = sizeof(theirAddr);
	printf("%s\n", "Waiting for sender...");
	if ((bytesRecvd = recvfrom(globalSocketUDP, recvBuf, 2048, 0,
				(struct sockaddr*)&theirAddr, &theirAddrLen)) == -1)
	{
		perror("connectivity listener: recvfrom failed");
		exit(1);
	}
	printf("%s\n", recvBuf);

}

int main(int argc, char** argv)
{
	unsigned short int udpPort;

	if(argc != 3)
	{
		fprintf(stderr, "usage: %s UDP_port filename_to_write\n\n", argv[0]);
		exit(1);
	}

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
	bindAddr.sin_port = htons(5000);
	inet_pton(AF_INET, myAddr, &bindAddr.sin_addr);
	if(bind(globalSocketUDP, (struct sockaddr*)&bindAddr, sizeof(struct sockaddr_in)) < 0)
	{
		perror("bind");
		close(globalSocketUDP);
		exit(1);
	}

	udpPort = (unsigned short int)atoi(argv[1]);

	reliablyReceive(udpPort, argv[2]);
}
