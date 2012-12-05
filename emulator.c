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

#define MAX_LINE 512
#define MAX_DELAYED_PACKETS 512

int debug = 1;
int PORT;
int QUEUE_SIZE;
char* LOG;
char* TABLE_NAME = "table.txt";
forwarding_entry_t* forwardingTable;
int numTableEntries = 0;

char* corArgText = "Please run the program with the correct arguments.\n\n-p:  The port number\n-q:  The queue size\n-f:  The file name\n-l:  The log file name\n";
char* corPortText = "Please choose a port number greater than 1024 and less than 65536.\n";

queue_t* priority1;
queue_t* priority2;
queue_t* priority3;

forward_info_t curr_delayed_packet;

forward_info_t delaying_packets[MAX_DELAYED_PACKETS]; //array of packets being delayed

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
	if (argc != 9)
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
		else if (strcmp(argType, "-q") == 0)
		{
			iArg = strtol(argv[i+1], &errorChk, 10);
			checkArg(errorChk,argv[i+1], argType);
			QUEUE_SIZE = iArg;
		}
		else if (strcmp(argType, "-f") == 0)
		{
			sArg = argv[i+1];
			TABLE_NAME = sArg;
		}
		else if (strcmp(argType, "-l") == 0)
		{
			sArg = argv[i+1];
			LOG = sArg;
		}
		else if ((i % 2) == 1)
		{
			/* An incorrect argument has been entered, print error and quit */
			perror(strcat(argType, " is not a valid argument.  Please retry with the correct arguments.\n"));
			die(corArgText);
		}
	}
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

int
buildTable()
{
	// Get the IP address of 
	char recv_name[MAX_LINE];
	gethostname(recv_name, sizeof(recv_name));
	int thisIP = get_IP(recv_name);

	// Put the forwarding table into an array
	// Open the forwarding table file for reading
	FILE* fp = fopen(TABLE_NAME, "rt");
	if (fp == NULL)
	{
	if(debug)	printf("Could not open forwarding table file %s.\n\n", TABLE_NAME);
		return 1;
	}

	// Read the file to determine the number of lines
	char line[MAX_LINE];
	while(fgets(line, sizeof(line), fp) != NULL)
	{
		numTableEntries++;
		memset(&line[0], 0, sizeof(line));
	}

	// Set the file pointer back to the start of the file
	fseek(fp, 0, SEEK_SET);

	// Allocate space in the array for the entries
	forwardingTable = malloc(numTableEntries * sizeof(forwarding_entry_t));

	// Break up the data on each line and add it to the array
	int index = 0;
	struct hostent* host;
	struct in_addr* hostAddr;
	int* ipInTable;
	while(fgets(line, sizeof(line), fp))
	{
		// Split up the line by spaces and put the data into a struct
		// Emulator
		if(debug) printf("%s\n", line);
		char emuMachine[MAX_LINE];
		strcpy(emuMachine, strtok(line, " "));
		if(debug) printf("emuMachine: %s\n", emuMachine);
		strcat(emuMachine, ".cs.wisc.edu");

		host = gethostbyname(emuMachine);
		hostAddr = (struct in_addr*)host->h_addr;
		ipInTable = (int*)hostAddr;

		forwardingTable[index].emuIP = *ipInTable;
		forwardingTable[index].emuPort = (short)atoi(strtok(NULL, " "));

		if(debug) printf("IP: %d\n", forwardingTable[index].emuIP);

		//If this listing isn't for this emulator, continue to the next listing
		if ((thisIP != *ipInTable) || (PORT != forwardingTable[index].emuPort))
		{
			memset(&line[0], 0, sizeof(line));
			continue;
		}

		// Destination
		char destMachine[MAX_LINE];
		strcpy(destMachine, strtok(NULL, " "));
		strcat(destMachine, ".cs.wisc.edu");

		host = gethostbyname(destMachine);
		hostAddr = (struct in_addr*)host->h_addr;
		ipInTable = (int*)hostAddr;

		forwardingTable[index].destIP = *ipInTable;
		forwardingTable[index].destPort = (short)atoi(strtok(NULL, " "));

		// Next Hop
		char nextHopMachine[MAX_LINE];
		strcpy(nextHopMachine, strtok(NULL, " "));
		strcat(nextHopMachine, ".cs.wisc.edu");

		host = gethostbyname(nextHopMachine);
		hostAddr = (struct in_addr*)host->h_addr;
		ipInTable = (int*)hostAddr;

		forwardingTable[index].nextHopIP = *ipInTable;
		forwardingTable[index].nextHopPort = (short)atoi(strtok(NULL, " "));

		// Delay
		forwardingTable[index].delay = atoi(strtok(NULL, " "));

		// Loss Probability
		forwardingTable[index].lossProb = atoi(strtok(NULL, " "));

		// Clear the array
		memset(&line[0], 0, sizeof(line));
		forwarding_entry_t a;
		a = forwardingTable[index];
		if(debug) printf("Entry #%d\nEmulator IP: %d,%d\nDest: %d,%d\nNext Hop: %d,%d\nDelay: %d\nLoss Prob: %d\n",index + 1, forwardingTable[index].emuIP,a.emuPort,a.destIP,a.destPort,a.nextHopIP,a.nextHopPort,a.delay,a.lossProb);
		if(debug) printf("______________________________________________________________\n");
		index++;
	}
	fclose(fp);

	return 0;
}

void logerror(fpacket_t packet, char* reason)
{
	printf("Logged %s\n", reason);
	//if(debug)	printf("Start logging\n");
	fflush(stdout);
	struct hostent *source;
	struct hostent *dest;
	int sourceIP;
	short sourcePort;
	int destIP;
	short destPort;
	char cPriority;
	int iPriority;
	int payloadSize;
	
	// Get info about source
	sourceIP = packet.fheader.src_address;
	sourcePort = packet.fheader.src_port;
	if((source = gethostbyaddr(&sourceIP, sizeof(destIP), AF_INET)) == NULL)
	{
		die("Cannot get source info\n");
	}

	// Get infor about destination
	destIP = packet.fheader.dest_address;
	destPort = packet.fheader.dest_port;
	if((dest = gethostbyaddr(&destIP, sizeof(destIP), AF_INET)) == NULL)
	{
		die("Cannot get dest info\n");
	}

	// Get the time of the loss to millisecond granularity
	time_t t;
	struct tm curTime;
	struct timeval ms;

	t = time(NULL);
	curTime = *localtime(&t);
	gettimeofday(&ms, NULL);

	// Get the priority of the packet
	memcpy(&cPriority, &(packet.fheader), sizeof(char));
	iPriority = (int)cPriority;

	// Get the length of the payload

	payloadSize = packet.fheader.length;

	FILE* fp;
	fp = fopen(LOG, "a+");

	if (fp == NULL)
	{
		die("Could not open log file.\n");
		return;
	}

	fprintf(fp, "\n%s\nSource -- Name: %s, Port: %d\nDestination -- Name: %s, Port: %d\n", reason, source->h_name, sourcePort, dest->h_name, destPort);
	fprintf(fp, "Time of loss: %d-%d-%d %d:%d:%d.%d\n",curTime.tm_year + 1900, curTime.tm_mon + 1, curTime.tm_mday, curTime.tm_hour, curTime.tm_min, curTime.tm_sec, (int)(ms.tv_usec * .001));
	fprintf(fp, "Priority Level: %d\nSize of payload: %d\n", iPriority, payloadSize);
	fprintf(fp, "_____________________________________________________________\n");

	fflush(fp);
	fclose(fp);
}

//------------------------------------------ START SHER MINN -----------------------------------------------------//
//------------------------------------------ START SHER MINN -----------------------------------------------------//
//------------------------------------------ START SHER MINN -----------------------------------------------------//
//------------------------------------------ START SHER MINN -----------------------------------------------------//

//enqueues np 
int queue(fpacket_t fp, forwarding_entry_t fe)
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

	//check priority level and enqueue appropriately
	if((np->fheader).priority == 1)
	{
		full = enqueue(priority1, &f);
		//if(debug)
		// {
		// 	printf("Put packet in Queue 1\n");
		// 	printf("Queue 1 size : %d\n", priority1->size);
		// }
	}
	else if((np->fheader).priority == 2)
	{
		full = enqueue(priority2, &f) * 2;
		//if(debug)
		// {
		// 	printf("Put packet in Queue 2\n");
		// 	printf("Queue 2 size : %d\n", priority2->size);
		// } 
	}
	else if((np->fheader).priority == 3)
	{
		full = enqueue(priority3, &f) * 3;
		//if(debug) 
		// {
		// 	printf("Put packet in Queue 3\n");
		// printf("Queue 3 size : %d\n", priority3->size);
			
		// }
	}
	else
	{
		die("Error: Received packet with invalid priority\n");
	}
	//if(debug) printf("FORWARDING ENTRY:\nNEXT HOP: %d\nNEXT PORT: %d\n", f.host_addr, f.host_addr);

	//packet dropped if queue is full, log incident
	if(full < 0)
	{
		printf("Packet not put in queue; it was full\n");
		char errmsg [MAX_LINE];
		sprintf(errmsg, "Queue %d is full. Packet will dropped.\n", full * -1);
		logerror(*np, errmsg);
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

//randomly returns 1 or 0 based of probability of packet loss
int drop(int probability)
{
	int this_prob = rand() % 101;

	if(this_prob < probability)
	{
		if(debug) printf("Random value is %d; dropping packet\n", this_prob);
		return 0;
	}

	if(debug) printf("Random value is %d; NOT dropping packet\n", this_prob);	
	return 1;
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
	if(is_empty(priority1) != 0)
	{
		dequeue(priority1, f);
	}
	else if(is_empty(priority2) != 0)
	{
		dequeue(priority2, f);
	}
	else if(is_empty(priority3) != 0)
	{
		dequeue(priority3, f);
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

	//randomly determine if packet should be dropped if its not end packet

	if((f->packet).fpayload.header.type !='E' && (f->packet).fpayload.header.type !='R')
	{
		if(drop(f->prob_drop) != 1)
		{
			logerror(np, "Packet randomly dropped");
			return;
		}
	}

	//if(debug) printf("Create host data structure\n");
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
		logerror(packet, "No forwarding entry found.");
		return;
	}

	// We found a match, queue the packet for sending
	queue(packet, forwardingEntry);
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

	//create queues
	priority1 = init_queue(QUEUE_SIZE);
	priority2 = init_queue(QUEUE_SIZE);
	priority3 = init_queue(QUEUE_SIZE);

	buildTable();
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
			printf("Size of queues %d %d %d\n", priority1->size, priority2->size, priority3->size);
			if(debug) printf("Started delaying a packet at ");
			print_curr_time();
			has_curr_packet = true;
		}

	}
		exit(0);
	}
