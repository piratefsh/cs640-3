#include <stdbool.h>

#ifndef UTIL
#define UTIL

#define MAX_NETWORK_PACKET_SIZE 80000
#define MAX_PACKET_SIZE 5129
#define MAX_PAYLOAD 5120
#define MAX_LINE 512

typedef struct __header_t
{
	char type;
	int seq;
	int len;

} header_t;

typedef struct __packet_t
{
	header_t header;
	char payload[MAX_PAYLOAD];

} packet_t;

//single instance of a request to server
typedef struct __request_t
{
	char filename[512];
	char host[512];
	int id;
	int port;
} request_t;

typedef struct __fheader_t
{
	char priority;
	int src_address;
	short src_port;
	int dest_address;
	short dest_port;
	int length;
} fheader_t;

typedef struct __fpacket_t
{
	fheader_t fheader;
	packet_t fpayload;
} fpacket_t;

typedef struct __request_payload_t
{
	char filename[512];
	int window_size;
	
} request_payload_t;

typedef struct __forward_info_t
{
	fpacket_t packet;
	int host_addr;
	short host_port;
	int delay;
	int prob_drop;
	struct timeval time_sent;

} forward_info_t;

typedef struct __sent_packet_t
{
	fpacket_t sentPacket;
	int seqNo;
	int timesSent;
	int receivedAck;
	double timeout;
	struct timeval lastTimeExamined;
} sent_packet_t;

typedef struct __forwarding_entry_t
{
	int emuIP;
	short emuPort;
	int destIP;
	short destPort;
	int nextHopIP;
	short nextHopPort;
	int delay;
	int lossProb;
} forwarding_entry_t;

//a node in queue
typedef struct __node_t
{
	forward_info_t info;
	struct __node_t* next;
} node_t;

//linked list queue data structure
typedef struct __queue_t
{
	int size;			//number of packets
	int max_size;
	node_t* head; 	//first packet in queue
	node_t* tail; 	//last packet in queue

} queue_t;

queue_t* init_queue(int max_size);
int enqueue(queue_t* q, forward_info_t* p);
int dequeue(queue_t* q, forward_info_t* p);
int  is_empty(queue_t* q);
void print_queue(queue_t* q);

void print_network_packet_data(fpacket_t* np);

#endif
