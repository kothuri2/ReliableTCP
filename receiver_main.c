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

typedef struct Data {
	char* buffer;
	size_t size;
} data;

int sockfd; /* socket */
int clientlen; /* byte size of client's address */
struct sockaddr_in serveraddr; /* server's addr */
struct sockaddr_in clientaddr; /* client addr */
struct hostent *hostp; /* client host info */
char *hostaddrp; /* dotted decimal host addr string */
int optval; /* flag value for setsockopt */
socklen_t sendersize;
int bytesRecvd;
unsigned long long int bytesToWrite;
unsigned long sequence_base;
unsigned long sequence_max;
unsigned long startind;
unsigned long endind;

void reliablyReceive(unsigned short int myUDPport, char* destinationFile) {

	int fd = open(destinationFile, O_WRONLY | O_APPEND | O_TRUNC | O_CREAT);
	data* recDataBuffer = malloc(WINDOW_SIZE * sizeof(data));
	int i;
	int receivedMap[WINDOW_SIZE];
	for(i = 0; i < WINDOW_SIZE; i++)
		receivedMap[i] = 0;
	while(1)
	{
		unsigned char ackBuf [sizeof(int) + WINDOW_SIZE*sizeof(unsigned long)];
		int reqNum = 0;
		for(i = startind; i < endind; i++) {
			if(bytesToWrite == 0) {
				reqNum = 0;
				memcpy(ackBuf, &reqNum, sizeof(int));
				sendto(sockfd, ackBuf, sizeof(int), 0, (struct sockaddr*)&clientaddr, clientlen);
				for(i = 0; i < WINDOW_SIZE; i++) {
					if(receivedMap[i] != 0) {
						write(fd, recDataBuffer[i].buffer, recDataBuffer[i].size);
						free(recDataBuffer[i].buffer);
					}
				}
				free(recDataBuffer);
				close(fd);
				printf("%s\n", "Successfully Received File");
				return;
			}

			unsigned char recData[FRAME_SIZE];
			bytesRecvd = recvfrom(sockfd, recData, FRAME_SIZE, 0, (struct sockaddr*)&clientaddr, &clientlen);
			if(bytesRecvd == -1) {
				unsigned long j;
				for(j = sequence_base; j <= sequence_max; j++) {
					if(receivedMap[j%WINDOW_SIZE] == 0) {
						reqNum ++;
						memcpy(ackBuf+sizeof(int) + (reqNum-1)*sizeof(unsigned long), &j, sizeof(unsigned long));
					}
					startind = 0;
					endind = reqNum;
				}
				memcpy(ackBuf, &reqNum, sizeof(int));
				break;
			}
			unsigned long sequence_num = *((unsigned long *)(recData));
			int order = (sequence_num)%(WINDOW_SIZE);
			if(sequence_num < sequence_base || sequence_num > sequence_max || receivedMap[order] != 0) {
				reqNum = 0;
				memcpy(ackBuf, &reqNum, sizeof(int));
				memcpy(ackBuf + sizeof(int), &sequence_base, sizeof(unsigned long));
				printf("%lu\n", sequence_base);
				sendto(sockfd, ackBuf, sizeof(unsigned long)+sizeof(int), 0, (struct sockaddr*)&clientaddr, clientlen);
				i--;
				continue;
			}
			int datasize = 0;
			if(sequence_num == 0) {
				bytesToWrite = *((unsigned long long int*)(recData+sizeof(unsigned long)));
				datasize = bytesRecvd-(sizeof(unsigned long)+sizeof(unsigned long long int));
				bytesToWrite -= datasize;
				recDataBuffer[order].buffer = strdup((char*)(recData+sizeof(unsigned long)+sizeof(unsigned long long int)));
			} else {
				datasize = bytesRecvd-sizeof(unsigned long);
				bytesToWrite -= datasize;
				recDataBuffer[order].buffer = strdup((char*)(recData+sizeof(unsigned long)));
			}
			printf("Received Packet %lu\n", sequence_num);
			recDataBuffer[order].size = datasize;
			receivedMap[order] = 1;
			hostp = gethostbyaddr((const char *)&clientaddr.sin_addr.s_addr, sizeof(clientaddr.sin_addr.s_addr), AF_INET);
	    hostaddrp = inet_ntoa(clientaddr.sin_addr);
		}
		if(reqNum == 0) {
			memcpy(ackBuf, &reqNum, sizeof(int));
			for(i = 0; i < WINDOW_SIZE; i++) {
				receivedMap[i] = 0;
				write(fd, recDataBuffer[i].buffer, recDataBuffer[i].size);
				free(recDataBuffer[i].buffer);
			}
			sequence_base = sequence_max + 1;
			memcpy(ackBuf + sizeof(int), &sequence_base, sizeof(unsigned long));
			sequence_max = sequence_base + (WINDOW_SIZE - 1);
			startind = 0;
			endind = WINDOW_SIZE;
			sendto(sockfd, ackBuf, sizeof(unsigned long)+sizeof(int), 0, (struct sockaddr*)&clientaddr, clientlen);
		}
		else
			sendto(sockfd, ackBuf, reqNum*sizeof(unsigned long) + sizeof(int), 0, (struct sockaddr*)&clientaddr, clientlen);
	}
}

void setUpPortInfo(unsigned short int my_port) {
	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	struct timeval read_timeout;
	read_timeout.tv_sec = 0;
	read_timeout.tv_usec = 10;
	setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &read_timeout, sizeof(read_timeout));

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
	sequence_base = 0;
	sequence_max = WINDOW_SIZE - 1;
	startind = 0;
	endind = WINDOW_SIZE;

	if(argc != 3)
	{
		fprintf(stderr, "usage: %s UDP_port filename_to_write\n\n", argv[0]);
		exit(1);
	}

	udpPort = (unsigned short int)atoi(argv[1]);
	setUpPortInfo(udpPort);
	reliablyReceive(udpPort, argv[2]);
}
