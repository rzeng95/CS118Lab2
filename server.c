#include<stdio.h> 
#include<string.h> 
#include<stdlib.h> 
#include<arpa/inet.h>
#include<sys/socket.h>
#include <fcntl.h>
#include <time.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "packet.c"

#define PORT 4000

int nextBool(double probability)
{
    return rand() <  probability * ((double)RAND_MAX + 1.0);
}

void die(char *s)
{
    perror(s);
    exit(1);
}

int main(int argc, char* argv[]) {
	int sockfd, id_base;
	struct sockaddr_in serv_addr, cli_addr;
	struct packet incoming, outgoing;
	char *filename;
	FILE *myFile;
	int slen = sizeof(cli_addr);
	int r, s;

	fd_set readset;
	struct timeval timeout = {1, 0};
	srand(time(NULL));

	int numPacketsNeeded;
	int counter;
	if(argc != 4) { printf("Usage: ./server WindowSize PLoss PCorruption\n"); exit(1); }

	int windowSize = atoi(argv[1]);
	double loss = atof(argv[2]);
	double corrupt = atof(argv[3]);

	//handle opening socket etc
	sockfd=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if(sockfd < 0) die("socket()");

	bzero((char *) &serv_addr, sizeof(serv_addr));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	//bind socket to port
    if( bind(sockfd , (struct sockaddr*)&serv_addr, sizeof(serv_addr) ) < 0) { die("bind"); }

	listen(sockfd, 5);

	

	while (1) {

		printf("Waiting for REQUEST PACKET\n");
		bzero((char *) &incoming, sizeof(incoming));
		r = recvfrom(sockfd, &incoming, sizeof(incoming), 0, (struct sockaddr*) &cli_addr, (socklen_t*)&slen);
		if (r < 0) {printf("PACKET %d LOST\n", incoming.id); continue;}

		if (incoming.type != 0) { continue; }

		puts("\n=================================\n");
		puts("REQUEST PACKET HAS BEEN RECEIVED");
		filename = incoming.data;
		myFile = fopen(filename, "r");
		if (myFile == NULL) { die("requested file does not exist"); }
		printf("Requested file successfully found: %s\n",filename);

		//now find the size of the file so we can figure outgoing how many packets we need
		fseek(myFile, 0L, SEEK_END);
		int sz = ftell(myFile);
		fseek(myFile, 0L, SEEK_SET);
		printf("size of file: %d\n",sz);
		numPacketsNeeded = sz / PACKETSIZE;
		if (sz % PACKETSIZE > 0) numPacketsNeeded++; //account for remainder from division
		printf("number of packets needed: %d\n",numPacketsNeeded);
		puts("\n=================================\n");			

		counter = 0;
		id_base = 0;

		//send first packet
		bzero((char *) &outgoing, sizeof(outgoing));
		outgoing.type = 1;
		outgoing.id = counter;
		outgoing.size = fread(outgoing.data, 1, PACKETSIZE, myFile);
		fseek(myFile,PACKETSIZE*counter,SEEK_SET);
		fread(outgoing.data,1,PACKETSIZE,myFile);
		counter++;

		s = sendto(sockfd, &outgoing, sizeof(outgoing), 0, (struct sockaddr*) &cli_addr, slen);
		if (s<0) die("bad!");
		printf("PACKET %d HAS BEEN SENT\n",outgoing.id);
		printf("PACKET %d CONTAINS: %s\n", outgoing.id, outgoing.data);


		// start GBN procedure
		while (id_base < numPacketsNeeded) {
			FD_ZERO(&readset);
			FD_SET(sockfd, &readset);
			if (select(sockfd+1, &readset, NULL, NULL, &timeout) < 0) {
				die("select()");
			} else if (FD_ISSET(sockfd, &readset)) {

				r = recvfrom(sockfd, &incoming, sizeof(incoming), 0,(struct sockaddr*) &cli_addr, &slen);
				if (r < 0) { printf("PACKET %d LOST\n", incoming.id); counter = 0; continue; }

				if (nextBool(loss)) {
					printf("Simulated loss %d\n", incoming.id);
					counter = 0;
					continue;
				} else if (nextBool(corrupt)) {
					printf("Simulated corruption %d\n", incoming.id);
					counter = 0;
					continue;
				}

				if (incoming.type == 2 && incoming.id >= id_base && (incoming.id < id_base + windowSize)) {
					id_base = incoming.id + 1;
					printf("WINDOW SLIDE TO %d/%d\n", id_base, numPacketsNeeded);
					counter = 0;
				}
			} else {
				if (counter >= windowSize || id_base + counter >= numPacketsNeeded) {
					printf("Timeout on %d\n", id_base);
					counter = 0;
				} else if (id_base + counter >= numPacketsNeeded)
					continue;

				bzero((char *) &outgoing, sizeof(outgoing));
				outgoing.type = 1; 
				outgoing.id = id_base + counter;
				fseek(myFile, outgoing.id * PACKETSIZE, SEEK_SET);
				outgoing.size = fread(outgoing.data, 1, PACKETSIZE, myFile);

				// send next packet incoming window
				s= sendto(sockfd, &outgoing, sizeof(outgoing), 0, (struct sockaddr*) &cli_addr, slen);
				if (s < 0) die("sendto()");

				printf("PACKET %d HAS BEEN SENT\n",outgoing.id);
				printf("PACKET %d CONTAINS: %s\n", outgoing.id, outgoing.data);

				counter++;
			}
		}


		// send FIN PACKET
		bzero((char *) &outgoing, sizeof(outgoing));
		outgoing.type = 3; // FIN packet
		outgoing.id = id_base;
		outgoing.size = 0;
		s = sendto(sockfd, &outgoing, sizeof(outgoing), 0, (struct sockaddr*) &cli_addr, slen);
		if (s<0) die("bad!");
		printf("FIN HAS BEEN SENT\n");

		//Wait for FIN ACK
		while (1) {
			FD_ZERO(&readset);
			FD_SET(sockfd, &readset);
			if (select(sockfd+1, &readset, NULL, NULL, &timeout) < 0) {
				die("select()");
			} 
			else if (FD_ISSET(sockfd, &readset)) {

				r = recvfrom(sockfd, &incoming, sizeof(incoming), 0, (struct sockaddr*) &cli_addr, &slen);
				if (r < 0) continue;

				if (incoming.type == 3) {
					printf("FIN ACK HAS BEEN RECEIVED\n");
					break;				
				}
			} 
			else {
				s = sendto(sockfd, &outgoing, sizeof(outgoing), 0, (struct sockaddr*) &cli_addr, slen);
				if (s < 0) die("sendto()");

			}
		}

		puts("\n=================================\n");
		printf("All packets sent and acknowledged\n");
		fclose(myFile);
		close(sockfd);
		return 0;
		
	}
}
