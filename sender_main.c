#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <netdb.h>

int serverlen;
struct sockaddr_in serveraddr;
struct hostent *server;
int globalSocketUDP;
int WINDOW_SIZE = 4;
int PAYLOAD_SIZE = 1472;
double TIMEOUT_WINDOW = 100.0;
int sequence_base;
int sequence_max;
int maxposs;
int sendFlag = 0; // send when sendFlag is 0, don't send when 1
pthread_mutex_t mtx;
pthread_cond_t cv;
int numberOfFrames;
typedef struct Frame {
	char buf[1472];
  	int sequence_number;
  	struct timeval lastSent;
} frame;
frame * allFrames;

void* timeout(void * unusedParam) {
	while(1) {
		// Iterate through current window and see if any packets have timed out
		// If any packet has timed out, resend entire window
		int i = sequence_base;
		for(; i <= i+(WINDOW_SIZE-1); i++) {
			if(allFrames[i%(maxposs+1)].lastSent.tv_sec != -1) {
				struct timeval currentTime;
				gettimeofday(&currentTime, 0);
				double elapsed_time = (currentTime.tv_sec - allFrames[i%(maxposs+1)].lastSent.tv_sec) * 1000.0;
				elapsed_time += (currentTime.tv_usec - allFrames[i%(maxposs+1)].lastSent.tv_usec) / 1000.0;
				if(elapsed_time >= TIMEOUT_WINDOW) {
					// Did not receive ACK, resend the entire window
					printf("Packet %d timed out\n", allFrames[i].sequence_number);
					int j = sequence_base;
					for (; j <= j+(WINDOW_SIZE-1); j++) {
						sendto(globalSocketUDP, allFrames[j%(maxposs+1)].buf, sizeof(allFrames[j%(maxposs+1)].buf), 0, (struct sockaddr*)&serveraddr, serverlen);
					}
				}
			}
		}
	}
}

void* receiveAcks(void * unusedParam) {
	unsigned char recvBuf [8];
	int bytesRecvd;

	while(1) {
		bytesRecvd = recvfrom(globalSocketUDP, recvBuf, 8, 0, (struct sockaddr*)&serveraddr, &serverlen);
		//Received an ACK
		int request_number = *((int *) recvBuf);
//		printf("reqNum: %d\n", request_number);
		pthread_mutex_lock(&mtx);
		if((sequence_base+(WINDOW_SIZE-1)) > maxposs) {
			if(sequence_base<=request_number<=maxposs) {
				numberOfFrames -= (request_number - sequence_base);
				sequence_base = request_number;
				sequence_max = (request_number+(WINDOW_SIZE-1))%(maxposs+1);
			} else {
				numberOfFrames -= (((maxposs+1)-sequence_base)+1);
				numberOfFrames -= request_number;
				sequence_base = request_number;
				sequence_max = (request_number+(WINDOW_SIZE-1))%(maxposs+1);
			}
		} else {
			if(sequence_base <= request_number <= sequence_max) {
				//printf("NumFrames: %d\n", numberOfFrames);
				numberOfFrames -= (request_number-sequence_base);
				sequence_base = request_number;
				sequence_max = (request_number+(WINDOW_SIZE-1))%(maxposs+1);
			}
		}

		printf("Received an ACK for packet %d of %d\n", request_number, numberOfFrames);
		sendFlag = 0;
		pthread_cond_signal(&cv);
		if(request_number == numberOfFrames) {
			pthread_mutex_unlock(&mtx);
			break;
		}
		pthread_mutex_unlock(&mtx);
	}
}

void reliablyTransfer(char* hostname, unsigned short int hostUDPport, char* filename, unsigned long long int bytesToTransfer)
{
	int numBytesToRead = PAYLOAD_SIZE - sizeof(int);
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

	allFrames = malloc(numberOfFrames*(sizeof(frame)));

	//Initialize the frames;
	FILE * file = fopen(filename, "r");
	frame allFrames [maxposs+1];
	int acks [numberOfFrames];
	int firstflag = 1;
	int i;
	for (i = 0; i <= maxposs; i++) {
		allFrames[i].sequence_number = i;
		memcpy(allFrames[i].buf, &(allFrames[i].sequence_number), sizeof(int));
		allFrames[i].lastSent.tv_sec = -1;
	}

	pthread_t timeoutThread;
	pthread_create(&timeoutThread, 0, timeout, (void*)0);

	while(1) {
		pthread_mutex_lock(&mtx);
		while(sendFlag == 1)
			pthread_cond_wait(&cv, &mtx);

		if(numberOfFrames == 0)
		{
			pthread_mutex_unlock(&mtx);
			break;
		}
		int stopind = sequence_max;
		if(numberOfFrames < (WINDOW_SIZE-1))
			stopind = (sequence_base + (numberOfFrames-1))%(maxposs+1);
		i = sequence_base;
		for(; i <= (i+(WINDOW_SIZE-1)); i++) {
			// Transmit the packet
			if(firstflag) {
				if(numberOfFrames == 1 && lastPacketSize != -1) {
					memcpy(allFrames[i].buf+sizeof(int), &bytesToTransfer, sizeof(unsigned long long int));
					fread(allFrames[i].buf+sizeof(int)+sizeof(unsigned long long int), 1, lastPacketSize, file);
					printf("Sending packet %d of %d\n", allFrames[i].sequence_number, numberOfFrames);
					gettimeofday(&allFrames[i].lastSent, 0);
					sendto(globalSocketUDP, allFrames[i].buf, sizeof(int)+sizeof(unsigned long long int)+lastPacketSize, 0, (struct sockaddr*)&serveraddr, serverlen);
				} else {
					memcpy(allFrames[i].buf+sizeof(int), &bytesToTransfer, sizeof(unsigned long long int));
					fread(allFrames[i].buf+sizeof(int)+sizeof(unsigned long long int), 1, firstBytesToRead, file);
					printf("Sending packet %d of %d\n", allFrames[i].sequence_number, numberOfFrames);
					gettimeofday(&allFrames[i].lastSent, 0);
					sendto(globalSocketUDP, allFrames[i].buf, PAYLOAD_SIZE, 0, (struct sockaddr*)&serveraddr, serverlen);
				}
				firstflag = 0;
			} else {
				if(numberOfFrames == 1 && lastPacketSize != -1) {
					fread(allFrames[i%(maxposs+1)].buf+sizeof(int), 1, lastPacketSize, file);
					printf("Sending packet %d of %d\n", allFrames[i%(maxposs+1)].sequence_number, numberOfFrames);
					gettimeofday(&allFrames[i%(maxposs+1)].lastSent, 0);
					sendto(globalSocketUDP, allFrames[i%(maxposs+1)].buf, sizeof(int)+lastPacketSize, 0, (struct sockaddr*)&serveraddr, serverlen);
				} else {
					fread(allFrames[i%(maxposs+1)].buf+sizeof(int), 1, numBytesToRead, file);
					printf("Sending packet %d of %d\n", allFrames[i%(maxposs+1)].sequence_number, numberOfFrames);
					gettimeofday(&allFrames[i%(maxposs+1)].lastSent, 0);
					sendto(globalSocketUDP, allFrames[i%(maxposs+1)].buf, PAYLOAD_SIZE, 0, (struct sockaddr*)&serveraddr, serverlen);
				}
			}
			if(i%(maxposs+1) == stopind)
				break;
		}
		sendFlag = 1;
		pthread_mutex_unlock(&mtx);
	}

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
	sequence_max = WINDOW_SIZE-1;
	sequence_base = 0;
	maxposs = WINDOW_SIZE*2;

	pthread_mutex_init(&mtx, NULL);
	pthread_cond_init(&cv, NULL);

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
