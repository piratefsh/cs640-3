#include "dijkstra.h"
#include "linkstate.h"
#include "util.h"
#include <string.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <stdbool.h>

#ifndef LINKSTATEC
#define LINKSTATEC

static int debug = 1;
static int LINK_COST = 1;


void
make_host(char* h, host_t* host)
{
    char* saveptr_host;
    char* host_ip   = strtok_r(h, ",", &saveptr_host);
    char* host_port = strtok_r(NULL, "", &saveptr_host);

    memcpy(host->ip, host_ip, sizeof(host->ip));
    host->port = atoi(host_port);
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
    memcpy(ip, inet_ntoa(emulator.sin_addr), MAX_LINE);
    printf("My IP is %s\n", ip);

}


void
get_my_host_struct(host_t* h, int port)
{
    // Get my IP in dot notation
    char my_IP[MAX_LINE];
    get_my_IP(my_IP);

    memcpy(h->ip, my_IP, sizeof(my_IP));

    h->port     = port;
    h->online   = true;
}


void
print_host(host_t* h)
{
    printf("Host IP: %s     Host Port: %d\n", h->ip, h->port);
}


//builds topology table. returns true if this host is in table, false otherwise
int
read_topology(topology_table_t* table, char* topology_table)
{
    host_t this_host;
    get_my_host_struct(&this_host, 0);

    int found_this_host = 0; //to flag if this host is in table or not 

    FILE* fp;
    if((fp = fopen(topology_table, "rt")) == NULL)
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

        topology_entry_t entry;

        //read first host in entry
        char* h;
        char* saveptr_line;

        if((h = strtok_r(line, " ", &saveptr_line)))
        {
            //grab the node of this entry
            make_host(h, &(entry.node));
            print_host(&(entry.node));
            if(strcmp(entry.node.ip, this_host.ip) == 0)
            {
                found_this_host = 1;
            }
        }

        //grab neighbours and stuff them into entry
        int num_neighbours = 0;

        while ((h = strtok_r(NULL, " ", &saveptr_line)) != NULL)
        {
            make_host(h, &(entry.neighbours[num_neighbours]));
            num_neighbours++;
        }

        //record number of neighbours
        entry.num_neighbours = num_neighbours;

        //put entry in table
        memcpy(&(table->entries[num_nodes]), &entry, sizeof(topology_entry_t));
        num_nodes++;
    }                             

    table->num_nodes = num_nodes;

    if(debug)
        printf("Built topology table\n");
    return 0;
}




void
make_list_entry(host_t* this, host_t* from, list_entry_t* entry, int cost)
{
    entry->cost = cost;
    memcpy(&(entry->this_host), this, sizeof(host_t));
    memcpy(&(entry->reached_from), from, sizeof(host_t));
}

void
add_entry_to_list(list_entry_t* h, list_t* l)
{
    memcpy(&(l->entries[l->num_entries]), h, sizeof(list_entry_t));
    (l->num_entries)++;
}

void
update_list_entry(host_t* from, list_entry_t* entry, int cost)
{
    entry->cost = cost;
    entry->reached_from = *from;
}

void
print_neighbours(host_t* neighbours, int n)
{
    printf("NEIGHBOURS:\n");
    int i;
    for(i = 0; i < n; i++)
    {
        print_host(&(neighbours[i]));
    }
}

//get neighbouring nodes of node h
//returns number of neighbours if h exists in topology table,
//-1 if it doesn't exist
int
get_neighbours_of(host_t* h, host_t* neighbours, topology_table_t* table)
{
    //find entry in topology table for h
    int i;
    for(i = 0; i < table->num_nodes; i++){

        if(strcmp(table->entries[i].node.ip, h->ip) == 0)
        {
            memcpy(neighbours, &(table->entries[i].neighbours[0]), MAX_NODES * sizeof(host_t));

            //return number of neighbours
            return table->entries[i].num_neighbours;
        }
    }

    return -1;
}

bool
is_in(host_t* h, list_t* list, int num)
{
    int i;
    for(i = 0; i < num; i++)
    {
        if(strcmp((list->entries[i]).this_host.ip, h->ip) == 0)
        {
            return true;
        }
    }

    return false;
}

//returns pointer to entry for host h in list
list_entry_t*
get_entry(host_t* h, list_t* list)
{
    int i;
    for(i = 0; i < list->num_entries; i++)
    {
        if(strcmp((list->entries[i]).this_host.ip, h->ip) == 0)
        {
            return &(list->entries[i]);
        }
    }
    return NULL;
}

void
remove_entry(host_t* h, list_t* list)
{
    int i;
    int invalid_index;
    for(i = 0; i < list->num_entries; i++)
    {
        if(strcmp((list->entries[i]).this_host.ip, h->ip) == 0)
        {
            invalid_index = i;
        }
    }

    for(i = invalid_index+1; i < list->num_entries; i++)
    {
        list->entries[i-1] = list->entries[i];
    }

    list->num_entries--;
}

void
get_entry_with_lowest_cost(list_entry_t* lowest, list_t* l)
{
    int lowest_cost_index = 0;
    int i;
    for(i = 1; i < l->num_entries; i++)
    {
        if(l->entries[i].cost < l->entries[lowest_cost_index].cost)
        {
            lowest_cost_index = i;
        }
    }

    memcpy(lowest, &(l->entries[lowest_cost_index]), sizeof(list_entry_t));
}

void 
print_list(list_t* l)
{
    printf("        Node        Cost        Reached from\n");
    int i;
    for(i = 0; i < l->num_entries; i++)
    {
        list_entry_t le = l->entries[i];
        printf("    %s      %d          %s\n", le.this_host.ip, le.cost, le.reached_from.ip);
    }

}

int
calc_shortest_path(list_t* confirmed, topology_table_t* table, int port)
{
    if(debug)
        printf("Start calculating Dijkstra\n");

    list_t tentative;

    //  1. Initialize the Confirmed list with an entry for myself; this entry has a cost of 0.
    host_t h;
    get_my_host_struct(&h, port);

    list_entry_t e;
    make_list_entry(&h, &h, &e, 0);
    add_entry_to_list(&e, confirmed);

    host_t next;
    memcpy(&next, &h, sizeof(host_t));

    while(1){
        printf("\n");
        if(debug) 
        {
            printf("Next: ");
            print_host(&next);
        }
        // 2. For the node just added to the Confirmed list in the previous step,
        // call it node Next and select its LSP
        next = confirmed->entries[confirmed->num_entries-1].this_host;

        //get neighbours of next
        host_t neighbours[MAX_NODES];
        int num_neighbours = get_neighbours_of(&next, neighbours, table);

        // 
        // 3. For each neighbor (Neighbor) of Next, calculate the cost (Cost) to
        // reach this Neighbor as the sum of the cost from myself to Next and
        // from Next to Neighbor.

        //calculate cost outside of loop because all weights = 1
        list_entry_t next_entry = confirmed->entries[confirmed->num_entries-1];
        int cost = next_entry.cost + LINK_COST;
        
        int i;
        for(i = 0; i < num_neighbours; i++)
        {
            host_t curr = neighbours[i];
            print_host(&curr);

            // (a) If Neighbor is currently on neither the Confirmed nor the
            // Tentative list, then add (Neighbor, Cost, NextHop) to the
            // Tentative list, where NextHop is the direction I go to reach Next.

            if(!is_in(&curr, confirmed, confirmed->num_entries) && !is_in(&curr, &tentative, tentative.num_entries))
            {
                printf("Entry is not in Confirmed or Tentative. Added to Tentative.\n");
                list_entry_t ne; //neighbour entry
                make_list_entry(&curr, &next, &ne, cost);
                add_entry_to_list(&ne, &tentative);

            }

            // (b) If Neighbor is currently on the Tentative list, and the Cost is less than the currently listed cost for Neighbor, then replace the current entry with (Neighbor, Cost, NextHop), where NextHop is the direction I go to reach Next.
            else if(is_in(&curr, &tentative, tentative.num_entries))
            {
                printf("Entry is in Tentative\n");
                list_entry_t* ne; //list entry
                ne = get_entry(&curr, &tentative);

                if(ne != NULL && ne->cost > cost)
                {
                    printf("Cost is less than currently listed\n");
                    update_list_entry(&(next_entry.reached_from), ne, cost);
                }
            }
            else{
                printf("Entry is in Confirmed\n");
            }
        }

    // 4. If the Tentative list is empty, stop. Otherwise, pick the entry from
    // the Tentative list with the lowest cost, move it to the Confirmed list,
    // and return to step 2.
        printf("CONFIRMED:\n");
        print_list(confirmed);
        printf("TENTATIVE:\n");
        print_list(&tentative);
        if(tentative.num_entries <= 0)
        {
            break;
        }
        else 
        {
            list_entry_t lowest;
            get_entry_with_lowest_cost(&lowest, &tentative); 

            add_entry_to_list(&lowest, confirmed);

            remove_entry(&(lowest.this_host), &tentative);
        }
    }

    print_list(confirmed);
    return 0;

}

#endif