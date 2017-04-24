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

int globalSocketUDP;
int WINDOW_SIZE = 4;
int PAYLOAD_SIZE = 1472;
struct sockaddr_in receiver;
int sequence_base = 0;
int sequence_max;
int sendFlag = 0; // send when sendFlag is 0, don't send when 1
pthread_mutex_t sendFlagMutex;

typedef struct Frame {
	int sequence_number;
	char * data;
} frame;

void* receiveAcks(void * unusedParam) {
	struct sockaddr_in theirAddr;
	socklen_t theirAddrLen;
	unsigned char recvBuf [8];
	int bytesRecvd;

	while(1) {
		theirAddrLen = sizeof(theirAddr);
		if ((bytesRecvd = recvfrom(globalSocketUDP, recvBuf, sizeof(int), 0,
					(struct sockaddr*)&theirAddr, &theirAddrLen)) == -1)
		{
			perror("connectivity listener: recvfrom failed");
			exit(1);
		}

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
	int lastPacketSize = bytesToTransfer - (numberOfFrames*numBytesToRead);
	FILE * file = fopen(filename, "r");
	frame allFrames [numberOfFrames];
	int acks [numberOfFrames];
	int i;
	for (i = 0; i < numberOfFrames; i++) {
		allFrames[i].sequence_number = i;
		acks[i] = 0;
	}

	while(1) {
		if(sendFlag == 0) {
			if(sequence_base == (numberOfFrames - 1) ) {
				break;
			}
			i = sequence_base;
			for(; i <= sequence_max; i++) {
				if(sequence_base <= allFrames[i].sequence_number && allFrames[i].sequence_number <= sequence_max) {
					// Transmit the packet
					if(i != (numberOfFrames - 1)) {
						fread(allFrames[i].data,1,numBytesToRead,file);
					} else {
						fread(allFrames[i].data,1,lastPacketSize,file);
					}
					sendto(globalSocketUDP, ((const void *) &allFrames[i]), sizeof(allFrames[i]), 0, (struct sockaddr*)&receiver, sizeof(receiver));
				}
			}
			pthread_mutex_lock(&sendFlagMutex);
			sendFlag = 1;
			pthread_mutex_unlock(&sendFlagMutex);
		}
	}
	printf("%s\n", "Successfuly transferred file!");
	fclose(file);

}

void setUpPortInfo(const char * receiver_hostname, unsigned short int receiver_port) {
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

	memset(&receiver, 0, sizeof(receiver));
	receiver.sin_family = AF_INET;
	receiver.sin_port = htons(receiver_port);
	inet_pton(AF_INET, receiver_hostname, &receiver.sin_addr);
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
	reliablyTransfer(argv[1], udpPort, argv[3], numBytes);

	pthread_t receiveAcksThread;
	pthread_create(&receiveAcksThread, 0, receiveAcks, (void*)0);
} 
