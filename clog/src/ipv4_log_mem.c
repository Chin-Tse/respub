#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<errno.h>

/*For inet_addr*/
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <time.h>
#include <unistd.h>
#include "ipv4_log.h"
#include "ipv4_log_mem.h"


struct mem_stack * portlist_stack = NULL;
struct mem_stack * tuple_stack = NULL;

int create_portlist_stack(void)
{
	uint64_t *pstack = NULL;
	char *pstck_mem = NULL;
	uint32_t i;
	uint32_t mem_size = sizeof(struct port_list) * PORTLIST_STACK_DEPTH + 
		sizeof(void *) * PORTLIST_STACK_DEPTH + sizeof(struct mem_stack);
	portlist_stack = calloc(1,mem_size);
	if(NULL == portlist_stack)
		return -1;
	portlist_stack->depth = PORTLIST_STACK_DEPTH;
	portlist_stack->index = 0;
	pstack = (uint64_t *)&portlist_stack->stack;


	pstck_mem = (char *)pstack + sizeof(void *) * PORTLIST_STACK_DEPTH;
	printf("stact = %p,pstack=%p,mem=%p\n",portlist_stack,pstack,pstck_mem);
	for(i = 0;i < PORTLIST_STACK_DEPTH;i++)
	{
		pstack[i] = (uint64_t)(pstck_mem + i * sizeof(struct port_list));
	}
	return 0;
}

void destroy_portlist_stack(void)
{
	free(portlist_stack);
	return;
}

int create_tuple_stack(void)
{
	uint64_t *pstack = NULL;
	char *pstck_mem = NULL;
	uint32_t i;
	uint32_t mem_size = sizeof(struct ipv4_tuple_hash) * TUPLE_STACK_DEPTH + 
		sizeof(void *) * TUPLE_STACK_DEPTH + sizeof(struct mem_stack);
	tuple_stack = calloc(1,mem_size);
	if(NULL == tuple_stack)
		return -1;
	tuple_stack->depth = TUPLE_STACK_DEPTH;
	tuple_stack->index = 0;

	pstack = (uint64_t *)(&tuple_stack->stack);
	pstck_mem = (char *)pstack + sizeof(void *) * TUPLE_STACK_DEPTH;
	for(i = 0;i < TUPLE_STACK_DEPTH;i++)
	{
		pstack[i] = (uint64_t)(pstck_mem + i * sizeof(struct ipv4_tuple_hash));
	}
	return 0;
}

void destroy_tuple_stack(void)
{
	free(tuple_stack);
	return;
}
struct port_list * calloc_portlist(void)
{
	struct port_list *portlist;
	/*Try to get memory from stack first,and set the flgs*/
	if(portlist_stack->index < portlist_stack->depth)
	{
		portlist = (struct port_list *)(portlist_stack->stack[portlist_stack->index]);
		portlist_stack->index++;
		memset(portlist,0,sizeof(struct port_list));
		portlist->mem_flag = MEM_STACK_FLAG;
		return portlist;
	}
	portlist = calloc(1,sizeof(struct port_list));
	if(NULL == portlist)
		return NULL;
	portlist->mem_flag = 0;
	return portlist;
}
void free_portlist(struct port_list *portlist)
{
	/*If the memory is got from stack,free it to the stack*/
	if((portlist->mem_flag == MEM_STACK_FLAG) && (portlist_stack->index > 0))
	{
		portlist_stack->index--;
		portlist_stack->stack[portlist_stack->index] = (char *)portlist;
		return;
	}
	/*Others,free it to system*/
	free(portlist);
	return;
}


struct ipv4_tuple_hash * calloc_tuple(void)
{
	struct ipv4_tuple_hash *tuple_hash;
	/*Try to get memory from stack first,and set the flgs*/
	if(tuple_stack->index < tuple_stack->depth)
	{
		tuple_hash = (struct ipv4_tuple_hash *)(tuple_stack->stack[tuple_stack->index]);
		tuple_stack->index++;
		memset(tuple_hash,0,sizeof(struct ipv4_tuple_hash));
		tuple_hash->mem_flag = MEM_STACK_FLAG;
		return tuple_hash;
	}
	tuple_hash = calloc(1,sizeof(struct ipv4_tuple_hash));
	if(NULL == tuple_hash)
		return NULL;
	tuple_hash->mem_flag = 0;
	return tuple_hash;
}
void free_tuple(struct ipv4_tuple_hash *tuple_hash)
{
	/*If the memory is got from stack,free it to the stack*/
	if((tuple_hash->mem_flag == MEM_STACK_FLAG) && (tuple_stack->index > 0))
	{
		tuple_stack->index--;
		tuple_stack->stack[tuple_stack->index] = (char *)tuple_hash;
		return;
	}
	/*Others,free it to system*/
	free(tuple_hash);
	return;
}

int create_mem_stack(void)
{
	if(create_portlist_stack() < 0)
		return -1;
	if(create_tuple_stack() < 0)
	{
		destroy_portlist_stack();
		return -1;
	}
	return 0;
}

void destroy_mem_stack(void)
{
	destroy_portlist_stack();
	destroy_tuple_stack();
	return;
}

