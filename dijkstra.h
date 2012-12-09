#ifndef DIJKSTRA
#define DIJKSTRA

#include "linkstate.h"

typedef struct __list_entry_t
{
	int cost;
	host_t this_host;
	host_t reached_from;

}list_entry_t;

typedef struct __list_t 
{
	int num_entries;
	list_entry_t entries[MAX_NODES];
}list_t;


int read_topology(topology_table_t* table, char* topology_table);
int calc_shortest_path(list_t* confirmed, topology_table_t* table, int port);
#endif