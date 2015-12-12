#define PACKETSIZE 20

struct packet {
	int id;
	int type; //0 for request 1 for data 2 for ack 3 for fin 
	int size;
	char data[PACKETSIZE];

};
