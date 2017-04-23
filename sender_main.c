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


void reliablyTransfer(char* hostname, unsigned short int hostUDPport, char* filename, unsigned long long int bytesToTransfer) {
	struct sockaddr_in receiver;
	char tempaddr[100];
	sprintf(tempaddr, "127.0.0.1");
	memset(&receiver, 0, sizeof(receiver));
	receiver.sin_family = AF_INET;
	receiver.sin_port = htons(5000);
	inet_pton(AF_INET, tempaddr, &receiver.sin_addr);
	sendto(globalSocketUDP, "hello", 6, 0, (struct sockaddr*)&receiver, sizeof(receiver));
}

int main(int argc, char** argv)
{
	unsigned short int udpPort;
	unsigned long long int numBytes;

	if(argc != 5)
	{
		fprintf(stderr, "usage: %s receiver_hostname receiver_port filename_to_xfer bytes_to_xfer\n\n", argv[0]);
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
	bindAddr.sin_port = htons(9000);
	inet_pton(AF_INET, myAddr, &bindAddr.sin_addr);
	if(bind(globalSocketUDP, (struct sockaddr*)&bindAddr, sizeof(struct sockaddr_in)) < 0)
	{
		perror("bind");
		close(globalSocketUDP);
		exit(1);
	}

	udpPort = (unsigned short int)atoi(argv[2]);
	numBytes = atoll(argv[4]);

	reliablyTransfer(argv[1], udpPort, argv[3], numBytes);
}
