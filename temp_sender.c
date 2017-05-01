#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <netdb.h>

#define WINDOW_SIZE 4
#define PAYLOAD_SIZE 1472

int serverlen;
struct sockaddr_in serveraddr;
struct hostent *server;
int globalSocketUDP;
double TIMEOUT_WINDOW = 1000.0;
struct timeval ackTimer;

unsigned long sequence_base;
unsigned long sequence_max;
int sendFlag = 0; // send when sendFlag is 0, don't send when 1
pthread_mutex_t mtx;
pthread_cond_t cv;
pthread_cond_t ackcv;
unsigned long numberOfFrames;

typedef struct Frame {
	unsigned char buf[PAYLOAD_SIZE];
	size_t size;
} frame;

frame * allFrames;

void* receiveAcks(void * unusedParam) {
	unsigned char recvBuf [sizeof(unsigned long)];
	int bytesRecvd;
	int firstack = 1;

	while(1) {
		pthread_mutex_lock(&mtx);
		while(sendFlag == 0)
			pthread_cond_wait(&ackcv, &mtx);

		bytesRecvd = recvfrom(globalSocketUDP, recvBuf, sizeof(unsigned long), 0, (struct sockaddr*)&serveraddr, &serverlen);
		if(bytesRecvd == -1) {
			sendFlag = 0;
			pthread_cond_signal(&cv);
			pthread_mutex_unlock(&mtx);
			printf("Resending entire window from %lu - %lu / %lu \n", sequence_base, sequence_max, numberOfFrames);
			continue;
		}
		//Received an ACK
		int numToSend = *((int*)recvBuf);
		unsigned long val = *((unsigned long*)recvBuf);
		if(numToSend == -1) {
			//printf("%d %lu\n",numToSend, val);
			printf("Received a Complete ACK for %lu - %lu / %lu \n", sequence_base, sequence_max, numberOfFrames);
			sequence_base = sequence_max + 1;
			sequence_max = sequence_base + (WINDOW_SIZE-1);
			sendFlag = 0;
			pthread_cond_broadcast(&cv);
			if(sequence_max == (numberOfFrames -1)) {
				pthread_mutex_unlock(&mtx);
				break;
			}
		} else {
			unsigned long lostPacket = *((unsigned long*)recvBuf);
			printf("Received a Request ACK for %lu\n", lostPacket);
			sendto(globalSocketUDP, allFrames[lostPacket%WINDOW_SIZE].buf, allFrames[lostPacket%WINDOW_SIZE].size, 0, (struct sockaddr*)&serveraddr, serverlen);
			printf("Resending packet %lu\n", lostPacket);
		}
		// int i;
		// for(i = 0; i < numToSend; i++) {
		// 	long reqNum = *((unsigned long *)(recvBuf + sizeof(int) + (sizeof(unsigned long) * i) ));
		// 	printf("Resending Packet %lu\n", reqNum);
		// 	sendto(globalSocketUDP, allFrames[reqNum%WINDOW_SIZE].buf, allFrames[reqNum%WINDOW_SIZE].size, 0, (struct sockaddr*)&serveraddr, serverlen);
		// }

		pthread_mutex_unlock(&mtx);
	}
}

void reliablyTransfer(char* hostname, unsigned short int hostUDPport, char* filename, unsigned long long int bytesToTransfer) {
	int numBytesToRead = PAYLOAD_SIZE - sizeof(unsigned long);
	int firstBytesToRead = numBytesToRead - sizeof(unsigned long long int);
	int lastPacketSize = -1;
	if(bytesToTransfer > firstBytesToRead) {
		numberOfFrames = (bytesToTransfer-firstBytesToRead)/numBytesToRead + 1;
		if((bytesToTransfer-firstBytesToRead)%numBytesToRead != 0)
		{
			lastPacketSize = bytesToTransfer - firstBytesToRead - ((numberOfFrames-1)*numBytesToRead);
			numberOfFrames++;
		}
	}
	else {
		numberOfFrames = 1;
		lastPacketSize = bytesToTransfer;
	}

	//Initialize the frames;
	int fd = open(filename, O_RDONLY);
	//FILE * fd = fopen("test.txt", "w");
    allFrames = malloc(WINDOW_SIZE*sizeof(frame));

	while(1) {
		pthread_mutex_lock(&mtx);
		while(sendFlag == 1) {
			//printf("here2\n");
			pthread_cond_wait(&cv, &mtx);
			//printf("woken up\n");
		}
		if(sequence_base == numberOfFrames)
		{
			pthread_mutex_unlock(&mtx);
			break;
		}
		if(sequence_max > (numberOfFrames-1))
			sequence_max = numberOfFrames-1;
		unsigned long i = sequence_base;
		for(; i <= sequence_max; i++) {
			// Transmit the packet
			memcpy(allFrames[i%WINDOW_SIZE].buf, &i, sizeof(unsigned long));
			if(i == 0) {
				unsigned long long int * bytesTemp = malloc(sizeof(unsigned long long int));
				*bytesTemp = bytesToTransfer;
				memcpy(allFrames[i%WINDOW_SIZE].buf+sizeof(unsigned long), bytesTemp, sizeof(unsigned long long int));
				if(i == (numberOfFrames-1) && lastPacketSize != -1) {
					read(fd, allFrames[i%WINDOW_SIZE].buf+sizeof(unsigned long)+sizeof(unsigned long long int), lastPacketSize);
					//fread(allFrames[i%WINDOW_SIZE].buf+sizeof(unsigned long)+sizeof(unsigned long long int), 1, lastPacketSize, file);
					//fwrite(allFrames[i%WINDOW_SIZE].buf+sizeof(unsigned long)+sizeof(unsigned long long int), 1, lastPacketSize, fd);
					printf("Sending packet %lu of %lu\n", *((unsigned long*)allFrames[i%WINDOW_SIZE].buf), numberOfFrames);
					allFrames[i%WINDOW_SIZE].size = sizeof(unsigned long)+sizeof(unsigned long long int)+lastPacketSize;
				} else {
					read(fd, allFrames[i%WINDOW_SIZE].buf+sizeof(unsigned long)+sizeof(unsigned long long int), firstBytesToRead);
					//fread(allFrames[i%WINDOW_SIZE].buf+sizeof(unsigned long)+sizeof(unsigned long long int), 1, firstBytesToRead, file);
					//fwrite(allFrames[i%WINDOW_SIZE].buf+sizeof(unsigned long)+sizeof(unsigned long long int), 1, firstBytesToRead, fd);
					printf("Sending packet %lu of %lu\n",*((unsigned long*)allFrames[i%WINDOW_SIZE].buf), numberOfFrames);
					allFrames[i%WINDOW_SIZE].size = PAYLOAD_SIZE;
				}
			} else {
				if(i == (numberOfFrames-1) && lastPacketSize != -1) {
					read(fd,allFrames[i%WINDOW_SIZE].buf+sizeof(unsigned long), lastPacketSize);
					//fread(allFrames[i%WINDOW_SIZE].buf+sizeof(unsigned long), 1, lastPacketSize, file);
					//fwrite(allFrames[i%WINDOW_SIZE].buf+sizeof(unsigned long), 1, lastPacketSize, fd);
					//printf("Sending packet %lu of %lu\n", *((unsigned long*)allFrames[i%WINDOW_SIZE].buf), numberOfFrames);
					allFrames[i%WINDOW_SIZE].size = sizeof(unsigned long)+lastPacketSize;
				} else {
					read(fd,allFrames[i%WINDOW_SIZE].buf+sizeof(unsigned long), numBytesToRead);
					//fread(allFrames[i%WINDOW_SIZE].buf+sizeof(unsigned long), 1, numBytesToRead, file);
					//fwrite(allFrames[i%WINDOW_SIZE].buf+sizeof(unsigned long), 1, numBytesToRead, fd);
					//printf("Sending packet %lu of %lu\n", *((unsigned long*)allFrames[i%WINDOW_SIZE].buf), numberOfFrames);
					allFrames[i%WINDOW_SIZE].size = PAYLOAD_SIZE;
				}
			}
			sendto(globalSocketUDP, allFrames[i%WINDOW_SIZE].buf, allFrames[i%WINDOW_SIZE].size, 0, (struct sockaddr*)&serveraddr, serverlen);
		}

		sendFlag = 1;
		pthread_cond_signal(&ackcv);
		pthread_mutex_unlock(&mtx);
	}

	//printf("%s\n", "Successfuly transferred file!");
	close(fd);
	//fclose(fd);
	exit(0);
}

void setUpPortInfo(const char * receiver_hostname, unsigned short int receiver_port) {

	if((globalSocketUDP=socket(AF_INET, SOCK_DGRAM, 0)) < 0)
	{
		perror("socket");
		exit(1);
	}

	struct timeval read_timeout;
	read_timeout.tv_sec = 0;
	read_timeout.tv_usec = 100;
	setsockopt(globalSocketUDP, SOL_SOCKET, SO_RCVTIMEO, &read_timeout, sizeof(read_timeout));

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
	sequence_max = WINDOW_SIZE-1;
	sequence_base = 0;
	//ackTimer.tv_sec = -1;

	pthread_mutex_init(&mtx, NULL);
	pthread_cond_init(&cv, NULL);
	pthread_cond_init(&ackcv, NULL);

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
