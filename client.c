#include<stdio.h> 
#include<string.h>
#include<stdlib.h>
#include<arpa/inet.h>
#include<sys/socket.h>
#include <time.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/types.h>

#include "packet.c"

#define SERVER "127.0.0.1"
#define PORT 4000

// Source: http://stackoverflow.com/questions/3771551/how-to-generate-a-boolean-with-p-probability-using-c-rand-function
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
	int sockfd, counter;
	struct sockaddr_in serv_addr;
	struct hostent *server;
	char *hostname, *filename;

	socklen_t serv_len;
	struct packet outgoing, incoming;
	FILE *copiedFile;

	int r, s;
	
	srand(time(NULL));

	if(argc != 4) { printf("Usage : ./client filename pL pC\n"); exit(1); }
	filename = argv[1];
	double pLoss = atof(argv[2]);
	double pCorrupt = atof(argv[3]);
	if (pLoss < 0 || pLoss > 1 || pCorrupt < 0 || pCorrupt > 1) { printf("Invalid values used!\n"); exit(1); }

	//handle opening socket etc
	sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if(sockfd < 0) die("socket()");
	server = gethostbyname(SERVER);
	serv_addr.sin_family = AF_INET;
	serv_len = sizeof(serv_addr);
	bzero((char *) &serv_addr, serv_len);
	bcopy((char *) server->h_addr, (char *) &serv_addr.sin_addr.s_addr,server->h_length);
	serv_addr.sin_port = htons(PORT);

	//create a request packet to the server to fetch given file name
	bzero((char *) &outgoing, sizeof(outgoing));
	outgoing.type = 0;
	outgoing.id = 0;
	outgoing.size = sizeof(filename)+1;
	//puts(filename);
	strcpy(outgoing.data, filename);

	//send the request packet
	s = sendto(sockfd, &outgoing, sizeof(outgoing), 0, (struct sockaddr*) &serv_addr, serv_len);
	if (s < 0) die("request");
	puts("\n=================================\n");
	puts("REQUEST PACKET HAS BEEN SENT");

	counter = 0;
	copiedFile = fopen("CopiedFile.txt", "w");

	bzero((char *) &outgoing, sizeof(outgoing));
	outgoing.type = 2;
	outgoing.id = counter - 1;
	outgoing.size = 0;


	while (1) {
		bzero((char *) &incoming, sizeof(incoming));
		r = recvfrom(sockfd, &incoming, sizeof(incoming), 0, (struct sockaddr*) &serv_addr, &serv_len);
		if (r < 0) die("recvfrom()");

		if (nextBool(pLoss)) {
			printf("LOST PACKET %d\n", incoming.id);
			continue;
		}
		else if (nextBool(pCorrupt)) {
			printf("CORRUPTED PACKET %d\n", incoming.id);
			//send ack again
			//bzero((char *) &outgoing, sizeof(outgoing));
			s = sendto(sockfd, &outgoing, sizeof(outgoing), 0, (struct sockaddr*) &serv_addr, serv_len);
			if (s<0) die("sendto()");
			printf("RE ACK %d HAS BEEN SENT\n",outgoing.id);
			continue;
		}
		

		// check packet number and type
		if (incoming.id > counter) {
			printf("IGNORE %d: expected %d\n", incoming.id, counter);
			continue;
		} else if (incoming.id < counter) {
			printf("IGNORE %d: expected %d\n", incoming.id, counter);
			outgoing.id = incoming.id; // send ACK anyways
		} else {
			if (incoming.type == 3) {
				// FIN signal received
				break;
			} else if (incoming.type != 1) {
				printf("IGNORE %d: not a data packet\n", incoming.id);
				continue;
			}

			printf("PACKET %d HAS BEEN RECEIVED\n",incoming.id);
			printf("PACKET %d CONTAINS: %s\n", incoming.id, incoming.data);

			fwrite(incoming.data, 1, incoming.size, copiedFile);
			outgoing.id = counter;
			counter++;
		}

		s = sendto(sockfd, &outgoing, sizeof(outgoing), 0, (struct sockaddr*) &serv_addr, serv_len);
		if (s<0) die("sendto()");
		printf("ACK %d HAS BEEN SENT\n",outgoing.id);

	} //end big while loop

	bzero((char *) &outgoing, sizeof(outgoing));
	outgoing.type = 3;
	outgoing.id = counter;
	s = sendto(sockfd, &outgoing, sizeof(outgoing), 0, (struct sockaddr*) &serv_addr, serv_len);
	if (s<0) die("sendto()");
	printf("FIN ACK [Type = %d] HAS BEEN SENT\n", outgoing.type);

	fclose(copiedFile);
	close(sockfd);
	return 0;
}
