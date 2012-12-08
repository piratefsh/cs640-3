#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <strings.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>
#include <sys/time.h>
#include "util.h"
#include "linkstate.h"

#define MAX_LINE 512
#define MAX_DELAYED_PACKETS 512

int debug = 1;
int PORT;
int QUEUE_SIZE;
char* TOPOLOGY = "topology.txt";
forwarding_entry_t* forwardingTable;
int numTableEntries = 0;

char* corArgText = "Please run the program with the correct arguments.\n\n-p:  The port number\nf:  The file name\n";
char* corPortText = "Please choose a port number greater than 1024 and less than 65536.\n";

queue_t* queue;

forward_info_t curr_delayed_packet;
forward_info_t delaying_packets[MAX_DELAYED_PACKETS]; //array of packets being delayed

topology_table_t table;

int delaying = 0;


/* Method: die
   Purpose: Prints an error message and quits.
   Parameters: s -- The message to print.
*/
void die(char* s)
{
	perror(s);
	exit(1);
}
void checkArg(char* errorChk, char* arg, char* argType)
{
	if (*errorChk != '\0')
	{
	if(debug)	printf("%s is not a valid value for argument %s.\n\n", arg, argType);
		die(corArgText);
	}
}

void checkArgs(int argc, char* argv[])
{
	int i;
	char* argType;
	int iArg;
	char* sArg;
	char* errorChk;
	
	/* Check for the correct number of arguments */
	if (argc != 5)
	{
		die(corArgText);
	}
	
	/* Initialize the server from the command line args */
	for (i = 0; i < argc - 1; i++)
	{
		argType = argv[i];

		if (strcmp(argType,"-p") == 0)
		{
			iArg = strtol(argv[i+1], &errorChk, 10);
			checkArg(errorChk,argv[i+1], argType);
			PORT = iArg;
			
			/* Check port, needs to be in range 1024<port<65536 */
			if (!(PORT > 1024) || !(PORT < 65536))
			{
				die(corPortText);
			}
		}
		else if (strcmp(argType, "-f") == 0)
		{
			sArg = argv[i+1];
			TOPOLOGY = sArg;
		}
		else if ((i % 2) == 1)
		{
			/* An incorrect argument has been entered, print error and quit */
			perror(strcat(argType, " is not a valid argument.  Please retry with the correct arguments.\n"));
			die(corArgText);
		}
	}

	//init variables
	QUEUE_SIZE = 1;
}

double getTimeDiff(struct timeval start_time, struct timeval end_time)
{
	// int ms_per_s = 1000;
	// int ms_per_mu = .001;
	int ms_per_s = 1000000;
	
	return (end_time.tv_sec * ms_per_s + end_time.tv_usec) - (start_time.tv_sec * ms_per_s + start_time.tv_usec);
	//(((end_time.tv_sec * ms_per_s) + (end_time.tv_usec * ms_per_mu)) - ((start_time.tv_sec * ms_per_s) + (start_time.tv_usec * ms_per_mu)));
}

//hostname name of host
//returns IP address in network host byte order
int
get_IP(char* hostname)
{
	//translate server hostname to peer IP
	struct hostent* hp;
	if((hp = gethostbyname(hostname)) == NULL)
	{
		die("Error: Bad hostname. Cannot get IP\n");
	}
	int* ip = (int*) (hp->h_addr);

	if(debug) printf("Hostname: %s\nIP:%d\n", hostname, *ip);
	
	// int ip;
	char dot_notation[MAX_LINE];
	if(inet_ntop(AF_INET, ip, dot_notation, MAX_LINE) == NULL) 
	{
		die("Error: Cannot convert dot-notation IP to binary\n");
	}

	 if(debug) printf("Dot notation: %s\n", dot_notation);

	return *ip;
}

FILE* 
open_file(char* filename)
{
	FILE* fp = fopen(filename, "rt");
	if (fp == NULL)
	{
		return NULL;
	}
	return fp;
}

void
make_host(char* h, host_t* host)
{
	char* saveptr_host;
	char* host_ip 	= strtok_r(h, ",", &saveptr_host);
	char* host_port	= strtok_r(NULL, "", &saveptr_host);

	if(debug)
		printf("%s, %s\n", host_ip, host_port);
	memcpy(host->ip, host_ip, sizeof(host->ip));
	host->port = atoi(host_port);
}

////----------------------------------------LINK STATE ROUTING------------------------------------////
int
read_topology()
{
	FILE* fp;
	if((fp = open_file(TOPOLOGY)) == NULL)
	{
		if(debug)
			printf("Topology table file cannot be opened\n");
		return -1;
	}

	int num_nodes = 0;

	char line[MAX_LINE]; //line buffer

	//read file
	while(fgets(line, sizeof(line), fp) != NULL)
	{
		//ignore empyt lines
		if(strlen(line) <= 1) 
			continue;

		if(debug)
			printf("Line: %s\n", line);

		topology_entry_t entry;

		//read first host in entry
		char* h;
		char* saveptr_line;

		if((h = strtok_r(line, " ", &saveptr_line)))
		{
			//grab the node of this entry
			make_host(h, &(entry.node));
		}

		//grab neighbours and stuff them into entry
		int num_neighbours = 0;

		while ((h = strtok_r(NULL, " ", &saveptr_line)) != NULL)
		{
			make_host(h, &(entry.neighbours[num_neighbours]));
			num_neighbours++;
		}

		//put entry in table
		memcpy(&(table.entries[num_nodes]), &entry, sizeof(topology_entry_t));
		num_nodes++;
	}                             

	table.num_nodes = num_nodes;
	if(debug)
		printf("Built topology table\n");
	return 0;
}

void
get_my_IP(char* ip)
{
	struct hostent* emulator_host;
	struct sockaddr_in emulator;

	char emulator_hostname[MAX_LINE];
	gethostname(emulator_hostname, sizeof(emulator_hostname));
	emulator_host = gethostbyname(emulator_hostname);

	bcopy(emulator_host->h_addr, (char*) &emulator.sin_addr, emulator_host->h_length);
	char* emulator_ip_dot_notation = inet_ntoa(emulator.sin_addr);
	printf("My IP is %s\n", emulator_ip_dot_notation);

	memcpy(ip, emulator_ip_dot_notation, sizeof(emulator_ip_dot_notation));
}

void
make_list_entry(host* this, host* fromm list_entry_t* entry)
{

}

int
calc_shortest_path()
{
	// Get my IP in dot notation
	char my_IP[MAX_LINE];
	get_my_IP(my_IP);

	list_t confirmed;

	// 	1. Initialize the Confirmed list with an entry for myself; this entry has
	//  a cost of 0.
	confirmed.entries[0] = 

	//  2. For the node just added to the Confirmed list in the previous step,
	// call it node Next and select its LSP
	// 
	// 3. For each neighbor (Neighbor) of Next, calculate the cost (Cost) to
	// reach this Neighbor as the sum of the cost from myself to Next and
	// from Next to Neighbor.

	// (a) If Neighbor is currently on neither the Confirmed nor the
	// Tentative list, then add (Neighbor, Cost, NextHop) to the
	// Tentative list, where NextHop is the direction I go to reach Next.

	// (b) If Neighbor is currently on the Tentative list, and the Cost is less
	// than the currently listed cost for Neighbor, then replace the
	// current entry with (Neighbor, Cost, NextHop), where NextHop
	// is the direction I go to reach Next.

	// 4. If the Tentative list is empty, stop. Otherwise, pick the entry from
	// the Tentative list with the lowthe lowest cost, move it to the Confirmed list,
	// and return to step 2.
	return 0;

}
////------------------------------------END LINK STATE ROUTING------------------------------------------////


//------------------------------------------ START SHER MINN -----------------------------------------------------//
//------------------------------------------ START SHER MINN -----------------------------------------------------//
//------------------------------------------ START SHER MINN -----------------------------------------------------//
//------------------------------------------ START SHER MINN -----------------------------------------------------//

//enqueues np 
int queue_packet(fpacket_t fp, forwarding_entry_t fe)
{
	//if(debug) printf("Start queueing\n");
	fpacket_t* np = &fp;

	int full = 0;

	forward_info_t f;

	f.packet 	= *np;
	f.host_addr = fe.nextHopIP;
	f.host_port = fe.nextHopPort;
	f.prob_drop = fe.lossProb;
	f.delay 	= fe.delay;

	//packet dropped if queue is full, log incident
	if((full = enqueue(queue, &f)) < 0)
	{
		printf("Packet not put in queue; it was full\n");
		char errmsg [MAX_LINE];
		sprintf(errmsg, "Queue %d is full. Packet will dropped.\n", full * -1);
		return -1;
	}

	return 0;
}

void delay(double delay_time)
{
	if(debug) printf("Starting delay for %fms\n", delay_time);
	delaying = 1;

	// Set up the send rate 
	struct timespec sleep_spec, rem_spec;
	sleep_spec.tv_sec = 0;

	// Set up seconds parameter
	while (delay_time > 999)
	{
		sleep_spec.tv_sec++;
		delay_time -= 1000;
	}

	sleep_spec.tv_nsec = (long)(delay_time * 1000000);

	nanosleep(&sleep_spec, &rem_spec);
	delaying = 0;
	if(debug) printf("Done delaying for %fms\n", delay_time);
}

//return -1 if f is NOT being delayed, 0 otherwise
int packet_being_delayed(forward_info_t* f)
{
	//get current time
	struct timeval curr_time;
	gettimeofday(&curr_time, NULL);

	//if have been waiting for longer than delay, not being delayed
	// int ms_per_s = 1000000;
	// if(debug) printf("Start delay at %lu , Now %lu\n", ((f->time_sent).tv_sec * ms_per_s + (f->time_sent).tv_usec), (curr_time.tv_sec * ms_per_s + curr_time.tv_usec));
	double had_been_delayed = getTimeDiff((f->time_sent), curr_time) * 0.001;

	// if(debug) printf("Delayed for %f / %d\n", had_been_delayed, f->delay);
	if(had_been_delayed >= f->delay )
	{
		return -1;
	}

	return 1;
}

int get_and_delay_packet(forward_info_t* f)
{

	//select packet from highest priority queue with packets
	if(is_empty(queue) != 0)
	{
		dequeue(queue, f);
	}
	else //if no packets anywhere
	{
		//if(debug) printf("No packets in any queues. Not sending anything\n");
		return -1;
	}

	//start delaying packet
	gettimeofday(&(f->time_sent), NULL);
	return 0;
}

//send a packet
void send_packet(int s, forward_info_t* f)
{

	//delay(f.delay);
	fpacket_t np = (f->packet);

	//create address structure 
	struct sockaddr_in host;
	host.sin_family	= AF_INET;
	host.sin_port 	= htons(f->host_port); 

	bcopy(&(f->host_addr), &host.sin_addr, sizeof(f->host_addr));
	//if(debug) printf("Try sending to host\n");

	//send packet
	if (sendto(s, &np, sizeof(np), 0, (struct sockaddr*)&host, sizeof(struct sockaddr_in)) < 0)
	{
		die("Error: Fail to send to host");
	}

	if(debug) printf("Done sending packet to %d, %d\n", f->host_addr, f->host_port);
	if(debug)print_network_packet_data(&np);
	return;
}


//------------------------------------------ END SHER MINN -----------------------------------------------------//
//------------------------------------------ END SHER MINN -----------------------------------------------------//
//------------------------------------------ END SHER MINN -----------------------------------------------------//
//------------------------------------------ END SHER MINN -----------------------------------------------------//

void route(fpacket_t packet)
{
	// Get the destination address and port from the packet
	int destAddr = packet.fheader.dest_address;
	short destPort = packet.fheader.dest_port;
	int nextHopIP;
	short nextHopPort;

	// Try to find a matching destination in the table
	forwarding_entry_t forwardingEntry;
	int destFound = 0;
	int i;
	for (i = 0; i < numTableEntries; i++)
	{
		forwardingEntry = forwardingTable[i];
		// printf("DEST ADDR: %d\n", destAddr);
		// printf("DEST PORT: %d\n", destPort);
		// Check if the destination address of the packet is present in the table 
		if ((forwardingEntry.destIP == destAddr) && (forwardingEntry.destPort == destPort)) 
		{
			destFound = 1;
			nextHopIP = forwardingEntry.nextHopIP;
			nextHopPort = forwardingEntry.nextHopPort;
			if(debug) printf("Found next hop: %d\n", i);
			break;
		}
	}

	// If no matching destination was found in the table, log the error
	if (destFound == 0)
	{
		if(debug)	printf("did not find next hop, going to log\n");
		return;
	}

	// We found a match, queue the packet for sending
	queue_packet(packet, forwardingEntry);
}


void 
print_curr_time()
{
	time_t t;
	t = time(NULL);
	struct tm curTime;
	struct timeval ms;
	curTime = *localtime(&t);
	gettimeofday(&ms, NULL);

	printf(" %d-%d-%d %d:%d:%d.%d\n",curTime.tm_year + 1900, curTime.tm_mon + 1, 
		curTime.tm_mday, curTime.tm_hour, curTime.tm_min, curTime.tm_sec, (int)(ms.tv_usec * .001));
}

/*
	Method: main
	Purpose: Performs a passive open and waits for requests for a file.  When a file is requested, fragments the file
			 into packets and sends them to the requester.
	Returns: 0 if successful
*/
int main(int argc, char* argv[])
{
	//--------------------------INIT -----------------------------------
	checkArgs(argc, argv);

	//create queue
	queue = init_queue(QUEUE_SIZE);

	//read topology table
	read_topology();
	calc_shortest_path();

	//-----------------------SOCKET OPENING-----------------------------
	//prepare emulator addr struct
	struct sockaddr_in emulator, host;
	struct hostent* hp;
	int sd;				//socket handle

	char emu_name [MAX_LINE];
	gethostname(emu_name, sizeof(emu_name));

	if((hp = gethostbyname(emu_name)) == NULL)
	{
		die("Error: Bad emulator hostname");
	}

	emulator.sin_family = AF_INET;
	emulator.sin_port 	= htons(PORT);
	emulator.sin_addr.s_addr = htonl(INADDR_ANY);


	//make socket and passive open
	if((sd = socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK, 0)) < 0)
	{
		die("Error: Failed to open socket");
	}

	//bind to socket
	if((bind(sd, (struct sockaddr*) &emulator, sizeof(emulator))) < 0)
	{
		die("Error: Failed to bind");
	}

	//-----------------------RECEIVE PACKETS-----------------------------
	
	bool has_curr_packet = false;
	//wait for packets
	while(1)
	{
		fpacket_t buf;
		int recvLen;
		socklen_t addr_len = sizeof(host);
		//wait for packet
		//if have packet, route it
		if ((recvLen = recvfrom(sd, (char*)&buf, sizeof(buf), 0, (struct sockaddr *)&host, &addr_len)) > 0)
		{
			if(debug) printf("Start routing\n");
			route(buf);
		}

		//if no packets arrive, check if current packet is being delayed
		if(has_curr_packet)
		{
			//if(debug) printf("Has a current packet\n");

			if(packet_being_delayed(&curr_delayed_packet) == 1)
			{
				//if(debug) printf("Packet being delayed\n");
				continue;
			}
			else
			{
				//if not being delayed anymore, send it
				if(debug) printf("Start sending packet at ");
				print_curr_time();
				if(debug)print_network_packet_data(&(curr_delayed_packet.packet));
				send_packet(sd, &curr_delayed_packet);
				has_curr_packet = false;
			}
		}
		//get next packet to delay
		//if no packet in queues, go wait for more packets to arrive
		else if(get_and_delay_packet(&curr_delayed_packet) >= 0)
		{
			if(debug) printf("Started delaying a packet at ");
			print_curr_time();
			has_curr_packet = true;
		}

	}
		exit(0);
	}
