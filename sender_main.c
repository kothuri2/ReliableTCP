#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <netdb.h>

//int sockfd, portno, n;
int serverlen;
struct sockaddr_in serveraddr;
struct hostent *server;
//char *hostname;

int globalSocketUDP;
int WINDOW_SIZE = 4;
int PAYLOAD_SIZE = 1472;
//struct sockaddr_in receiver;
int sequence_base = 0;
int sequence_max;
int sendFlag = 0; // send when sendFlag is 0, don't send when 1
pthread_mutex_t sendFlagMutex;

typedef struct Frame {
	int sequence_number;
	char * data;
} frame;

void* receiveAcks(void * unusedParam) {
	//struct sockaddr_in theirAddr;
	//socklen_t theirAddrLen;
	unsigned char recvBuf [8];
	int bytesRecvd;

	while(1) {
		//theirAddrLen = sizeof(theirAddr);
		bytesRecvd = recvfrom(globalSocketUDP, recvBuf, 8, 0, (struct sockaddr*)&serveraddr, &serverlen);
		//Received an ACK
		int request_number = *((int *) recvBuf);
		if (request_number > sequence_base) {
			sequence_max = (sequence_max - sequence_base) + request_number;
			sequence_base = request_number;
		}
		pthread_mutex_lock(&sendFlagMutex);
		sendFlag = 0;
		pthread_mutex_unlock(&sendFlagMutex);
	}
}

void reliablyTransfer(char* hostname, unsigned short int hostUDPport, char* filename, unsigned long long int bytesToTransfer) {
	//sendto(globalSocketUDP, "hello", 6, 0, (struct sockaddr*)&receiver, sizeof(receiver));
	int numBytesToRead = PAYLOAD_SIZE - sizeof(int);
	int numberOfFrames = bytesToTransfer/(numBytesToRead);
	if(bytesToTransfer%numBytesToRead != 0)
		numberOfFrames++;
	if(bytesToTransfer < numBytesToRead)
		numberOfFrames = 1;

	int lastPacketSize = bytesToTransfer - (numberOfFrames*numBytesToRead);
	FILE * file = fopen(filename, "r");
	frame allFrames [numberOfFrames];
	if(bytesToTransfer < numBytesToRead) {
		numberOfFrames = 1;
		lastPacketSize = bytesToTransfer;
	}
	int acks [numberOfFrames];
	int i;
	for (i = 0; i < numberOfFrames; i++) {
		allFrames[i].sequence_number = i;
		acks[i] = 0;
	}

	while(1) {
		if(sendFlag == 0) {
			if(sequence_base == (numberOfFrames - 1)) {
				break;
			}
			i = sequence_base;
			for(; i <= sequence_max; i++) {
				printf("%d %d %d\n", sequence_base, allFrames[i].sequence_number, sequence_max);
				if(sequence_base <= allFrames[i].sequence_number && allFrames[i].sequence_number <= sequence_max) {
					// Transmit the packet
					if(i != (numberOfFrames - 1)) {
						fread(allFrames[i].data,1,numBytesToRead,file);
					} else {
						fread(allFrames[i].data,1,lastPacketSize,file);
					}
					printf("Sending...\n");
					sendto(globalSocketUDP, ((const void *) &allFrames[i]), sizeof(allFrames[i]), 0, (struct sockaddr*)&serveraddr, serverlen);
				}
			}
			pthread_mutex_lock(&sendFlagMutex);
			sendFlag = 1;
			pthread_mutex_unlock(&sendFlagMutex);
		}
	}
	printf("here\n");
	fread(allFrames[sequence_base].data,1,numBytesToRead,file);
	printf("%s\n", "Successfuly transferred file!");
	fclose(file);

}

void setUpPortInfo(const char * receiver_hostname, unsigned short int receiver_port) {
	if((globalSocketUDP=socket(AF_INET, SOCK_DGRAM, 0)) < 0)
	{
		perror("socket");
		exit(1);
	}

	server = gethostbyname(receiver_hostname);
	if(server == NULL) {
		fprintf(stderr, "%s\n", "No such Host Name");
		exit(0);
	}

	/* build the server's Internet address */
  	bzero((char *) &serveraddr, sizeof(serveraddr));
  	serveraddr.sin_family = AF_INET;
  	bcopy((char *)server->h_addr,
  	(char *)&serveraddr.sin_addr.s_addr, server->h_length);
  	serveraddr.sin_port = htons(receiver_port);
	serverlen = sizeof(serveraddr);

}

int main(int argc, char** argv)
{
	unsigned short int udpPort;
	unsigned long long int numBytes;
	sequence_max = WINDOW_SIZE - 1;

	pthread_mutex_init(&sendFlagMutex, NULL);

	if(argc != 5)
	{
		fprintf(stderr, "usage: %s receiver_hostname receiver_port filename_to_xfer bytes_to_xfer\n\n", argv[0]);
		exit(1);
	}

	udpPort = (unsigned short int)atoi(argv[2]);
	setUpPortInfo((const char *)argv[1], udpPort);
	numBytes = atoll(argv[4]);

	pthread_t receiveAcksThread;
	pthread_create(&receiveAcksThread, 0, receiveAcks, (void*)0);

	reliablyTransfer(argv[1], udpPort, argv[3], numBytes);

}
