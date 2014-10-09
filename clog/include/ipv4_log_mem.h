#ifndef __IPV4_LOG_MEM_H__
#define __IPV4_LOG_MEM_H__

#define PORTLIST_STACK_DEPTH   32768
#define TUPLE_STACK_DEPTH 		65536
	
#define MEM_STACK_FLAG			0xA5A5

struct mem_stack{
	uint32_t depth;
	uint32_t index;
	char *stack[0];
};

struct port_list * calloc_portlist(void);
void free_portlist(struct port_list *portlist);
struct ipv4_tuple_hash * calloc_tuple(void);
void free_tuple(struct ipv4_tuple_hash *tuple_hash);
int create_mem_stack(void);
void destroy_mem_stack(void);

#endif /*__IPV4_LOG_MEM_H__*/

