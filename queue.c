#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "util.h"
#include <arpa/inet.h>


queue_t* init_queue(int max_size)
{
    queue_t* q = malloc(sizeof(queue_t));
    q->size = 0;
    q->max_size = max_size;

    return q;
}


//q queue for packet to be added to
//p packet to be added
//returns 0 if packet successfully added, -1 if queue is full
int 
enqueue(queue_t* q, forward_info_t* p)
{
    //check if queue is full
    if(q->size >= q->max_size)
    {
        return -1;
    }

    //make queue node
     node_t* n = malloc(sizeof( node_t));
    memcpy(&(n->info), p, sizeof(forward_info_t));
    n->next = NULL;

    //add to queue
    //if queue is empty
    if(q->size == 0)
    {
        q->head = (struct __node_t*)n;
        q->tail = n;
    }

    else
    {
         node_t* old_tail = q->tail;
        old_tail->next = n;
        q->tail = n;
    }

    ++(q->size);

    return 0;
}

//q queue to dequeue from
//p buffer for dequeued packet
//returns 0 if packet successfully dequeued, -1 if queue is empty
int
dequeue(queue_t* q, forward_info_t* p)
{
    //check if queue has any more packets
    if(q->size == 0){
        return -1;
    }

    //dequeue packet
    memcpy(p, &((q->head)->info), sizeof(forward_info_t));

    node_t* old_head = (q->head);
    q->head = old_head->next;

    free(old_head);
    --(q->size);

    return 0;
}

//returns 0 if queue is empty, 1 otherwise
int 
is_empty(queue_t* q)
{
    return q->size;
}

int
read_packet(packet_t* packet)
{
    printf("\nPacket:\n");
    header_t h                  = packet->header;
    char payload[h.len];
    memcpy(payload, packet->payload, h.len);

    printf(" type: %c\n sequence: %d\n length: %d\n", h.type, ntohl(h.seq), h.len);
    
    //If is request packet, print out request packet info
    if(h.type == 'R')
    {
        request_payload_t* r = (request_payload_t*) payload;

        printf(" File requested: %s\n Window size:%d\n", r->filename, r->window_size);
    }
    return 0;
}

void
print_network_packet_data(fpacket_t* np)
{
    printf("-----------------------------\n");
    fheader_t header = np->fheader;

    printf("Header:\n    Priority: %c\n    Src:%d\n    Src port:%d\n    Dest:%d\n    Dest port:%d\n    Length:%d\n", 
        (header.priority + '0'), header.src_address, header.src_port, header.dest_address, header.dest_port, header.length);
    read_packet(&(np->fpayload));
    printf("-----------------------------\n");
}

void
print_queue(queue_t* q)
{
    int i;
    node_t* curr = q->head;
    for(i = 0; i < q->size; i++)
    {
        print_network_packet_data(&((curr->info).packet));
        curr = curr->next;
    }
}

