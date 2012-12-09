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

#endif