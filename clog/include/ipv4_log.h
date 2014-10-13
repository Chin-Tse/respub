
#ifndef __IPV4_LOG_H__
#define __IPV4_LOG_H__

#include "list.h"
#include <stdarg.h>

#define LOG_ACCEPT 0
#define LOG_DROP -1

#define MAX_ITEM_LEN    		16
#define DEF_LOG_FILE  			"/var/log/ipv4.log"
#define DEF_STAT_FILE 			"/var/log/ipv4_stat.log"
#define STAT_LOG_DEF_FILE      	"/proc/net/ipv4_log"

#define STAT_LOG_FILE_MAX_SIZE 	104857600  /*100M*/
#define STAT_LOG_FILE_MAX_CNT  	10
#define STAT_DEF_EXPIRE 		60

enum ITEM{
	ITEM_SRC_IP = 0,
	ITEM_SRC_PORT,
	ITEM_DST_IP,
	ITEM_DST_PORT,
	ITEM_PROTOCOL,
	ITEM_PROTOCOL_NUM,
	ITEM_DEV,
	ITEM_RECV,
	ITEM_RCV_PKTS,
	ITEM_RCV_BYTES,
	ITEM_SEND,
	ITEM_SND_PKTS,
	ITEM_SND_BYTES,
	ITEM_SYN,
	ITEM_SYN_CNT,
	ITEM_SYNACK,
	ITEM_SYNACK_CNT,
	ITEM_CODE,
	ITEM_CODE_CNT,
	ITEM_TYPE,
	ITEM_TYPE_O,
	ITEM_TYPE_R,
	ITEM_MAX,
};

struct stat_info{
	uint32_t syn;
	uint32_t synack;
	uint32_t snd_p;
	uint32_t rcv_p;
	uint64_t snd_b;
	uint64_t rcv_b;
};

struct log_info{
	char if_name[MAX_ITEM_LEN];
	/*Protocol num*/
	uint8_t protocol;
	/*icmp code*/
	uint8_t code;
	/*for icmp original type*/
	uint8_t o_type;
	/*For icmp reply type*/
	uint8_t r_type;

	/*flow info*/
	uint32_t src;
	uint32_t dst;
	uint16_t sport;
	uint16_t dport;

	/*stat info*/
	struct stat_info stat;
};

struct ipv4_tuple {
	uint32_t s_ip;
	uint32_t d_ip;
	union{
		uint32_t all;
		struct{
			uint16_t s_port;
			uint16_t d_port;
		}tcp;
		struct{
			uint16_t s_port;
			uint16_t d_port;
		}udp;
		struct {
			uint8_t o_type; 
			uint8_t r_type;
			uint8_t code;
		} icmp;
	
	}u;
};

struct ipv4_tuple_hash{
	struct list_head entry;
	struct ipv4_tuple tuple;
	/*mem stack manage this flags,should not changed by user*/
	uint16_t mem_flag;
	uint16_t pad;
	/*Stat for this tuple*/
	struct stat_info stat;
};

struct port_list{
	struct list_head entry;
	struct list_head list;
	/*Total stat info for this port*/
	struct stat_info stat;
	/*Numbers of conntrack with this port*/
	uint32_t conn_num;
	uint16_t port;
	/*mem stack manage this flags,should not changed by user*/
	uint16_t mem_flag;
};

struct ipv4_log_stat_s{
	/*protocol num*/
	uint8_t protocol;
	/*protocol name*/
	char * name;
	/*port list for this protocol*/
	struct list_head list;
	/*match function for the log(we stat logs which matched)*/
	int (*log_match)(struct log_info *info);
	/*stat the logs after match*/
	int (*log_stat)(struct log_info *info);
	/*timeout process*/
	uint32_t (*time_out)(struct list_head *list);
	/*Find the tuple from the port list*/
	int (*tuple_stat)(struct port_list *portlist,struct ipv4_tuple *tuple,struct stat_info *info);
};
void log_out(const char *fmt, ...);

#endif /*__IPV4_LOG_H__*/
