#ifndef LINKSTATE
#define LINKSTATE

#define MAX_NODES 	20		//maximum number of nodes in the topology
#define IP_LEN 		128 		//number of chars in an IP address in dot notation
#include <stdbool.h>
//single host in network
typedef struct __host_t
{
	char ip[IP_LEN];
	short port;
	bool online;

}host_t;

//single entry in topology table for one node
typedef struct __topology_entry_t
{
	host_t node;	//node this entry is for
	host_t neighbours[MAX_NODES];
    int num_neighbours;
	int cost;

}topology_entry_t;

//topology table struct
typedef struct __topology_table_t
{
	int num_nodes;
	topology_entry_t entries [MAX_NODES];

}topology_table_t;

#endif