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



int tcp_match(struct log_info *info);
int tcp_stat(struct log_info *info);
int tcp_tuple_stat(struct port_list *portlist,struct ipv4_tuple *tuple,struct stat_info *stat);
uint32_t tcp_time_out(struct list_head *list );


struct ipv4_log_stat_s tcp_log_stat = {
	.protocol = IPPROTO_TCP,
	.name = "tcp",
	.log_match = tcp_match,
	.log_stat = tcp_stat,
	.time_out = tcp_time_out,
	.tuple_stat = tcp_tuple_stat,
};


/*Add the match here*/
int tcp_match(struct log_info *info)
{
//	if(info->dport == 80)
//		return LOG_DROP;
	return LOG_ACCEPT;
}

int tcp_stat(struct log_info *info)
{
	struct ipv4_tuple tuple;
	struct list_head *pos;
	struct port_list *portlist = NULL;

	/*Create tuple*/
	tuple.s_ip = info->src;
	tuple.d_ip = info->dst;
	tuple.u.tcp.s_port = info->sport;
	tuple.u.tcp.d_port = info->dport;

	/*Find tuple: list is empty,create it */
	if(list_empty(&tcp_log_stat.list))
	//if(NULL == tcp_log_stat.list)
	{
		/*Alloc new port list,and set head*/
		portlist = calloc_portlist();
		if(NULL == portlist)
			return -1;
		/*set it to be head*/
		list_add_tail(&portlist->entry,&tcp_log_stat.list);
		INIT_LIST_HEAD(&portlist->list);

		if(tcp_log_stat.tuple_stat(portlist,&tuple,&info->stat) < 0)
		{
			free_portlist(portlist);
			return -1;
		}
		/*Stat totol:*/
		portlist->stat = info->stat;
		portlist->port = info->dport;
		portlist->conn_num = 1;
		
	}
	/*list is exist,check if the tuple is already exist*/
	list_for_each(pos,&tcp_log_stat.list)
	{
		portlist = list_entry(pos,struct port_list,entry);
		if(portlist->port == info->dport){
			/*stat tuple and return*/
			if(tcp_log_stat.tuple_stat(portlist,&tuple,&info->stat) < 0)
			{
				return -1;
			}
			/*Stat totol:*/
			portlist->stat.snd_b += info->stat.snd_b;
			portlist->stat.snd_p += info->stat.snd_p;
			portlist->stat.rcv_b += info->stat.rcv_b;
			portlist->stat.rcv_p += info->stat.rcv_p;
			portlist->stat.syn += info->stat.syn;
			portlist->stat.synack += info->stat.synack;
			portlist->conn_num++;
			return 0;
		}
	}

	/*Not find: add new port list*/
	portlist = calloc_portlist();
	if(NULL == portlist)
		return -1;
	/*Add to tail*/
	list_add_tail(&portlist->entry,&tcp_log_stat.list);
	INIT_LIST_HEAD(&portlist->list);

	if(tcp_log_stat.tuple_stat(portlist,&tuple,&info->stat) < 0)
	{
		free_portlist(portlist);
		return -1;
	}
	portlist->stat = info->stat;
	portlist->port = info->dport;
	portlist->conn_num = 1;
	return 0;	
}

int tcp_tuple_stat(struct port_list *portlist,struct ipv4_tuple *tuple,struct stat_info *stat)
{
	struct ipv4_tuple *tcp_tuple;
	struct ipv4_tuple_hash *tuple_hash = NULL;
	struct list_head *pos;
	
	/*tuple hash list is empty*/
	//if(NULL == portlist->tuple_hash)
	if(list_empty(&portlist->list))
	{
		/*Alloc new tuple*/
		tuple_hash = calloc_tuple();
		if(NULL == tuple_hash)
		{
			return -1;
		}
		//INIT_LIST_HEAD(&(portlist->tuple_hash->entry));
		list_add_tail(&tuple_hash->entry,&portlist->list);
		/*set tuple*/
		tuple_hash->tuple = *tuple;
		tuple_hash->stat = *stat;
		return 0;
	}
	/*Not empty,try to find the tuple first*/
	list_for_each(pos,&portlist->list)
	{
		tuple_hash = list_entry(pos,struct ipv4_tuple_hash,entry);
		/*If we find,add tht stat to this tuple*/
		if(tuple_hash->tuple.d_ip == tuple->d_ip)
		{
			tuple_hash->stat.syn += stat->syn;
			tuple_hash->stat.synack += stat->synack;
			tuple_hash->stat.snd_b += stat->snd_b;
			tuple_hash->stat.snd_p += stat->snd_p;
			tuple_hash->stat.rcv_b += stat->rcv_b;
			tuple_hash->stat.rcv_p += stat->rcv_p;
			return 0;
		}
	}
	/*Not find:alloc new tuple hash and add tail*/
	tuple_hash = calloc_tuple();
	if(NULL == tuple_hash)
		return -1;
	/*set tuple*/
	tuple_hash->tuple = *tuple;
	tuple_hash->stat = *stat;
	list_add_tail(&tuple_hash->entry,&portlist->list);
	return 0;	
}

uint32_t tcp_time_out(struct list_head *list)
{
	struct port_list *portlist = NULL;
	struct list_head *pos,*n;
	struct list_head *hpos,*hn;
	struct ipv4_tuple_hash *tuple_hash = NULL;
	struct in_addr addr;
	uint32_t len,total_len = 0;
	char buf[1024];
	
	if(list_empty(list))
	{
		printf("empty\n");
		return 0;
	}
	list_for_each_safe(pos,n,list)
	{
		portlist = list_entry(pos,struct port_list,entry);
		if(!list_empty(&portlist->list))
		{
			len = snprintf(buf,1024,"D-port:%d, conn_num:%u, syn:%u, synack:%u, send:%u pkts %lu bytes,recv:%u pkts %lu bytes\n",
				portlist->port,portlist->conn_num,portlist->stat.syn,portlist->stat.synack,
				portlist->stat.snd_p,portlist->stat.snd_b,portlist->stat.rcv_p,portlist->stat.rcv_b);
			log_out("%s",buf);
			total_len += len;
			list_for_each_safe(hpos,hn,&portlist->list)
			{
				tuple_hash = list_entry(hpos,struct ipv4_tuple_hash,entry);
				//addr.sin_addr = tuple_hash->tuple.d_ip;
				memcpy(&addr,&(tuple_hash->tuple.d_ip),4);
				len = snprintf(buf,1024,"\t%s:%d syn:%u, synack:%u, send:%u pkts %lu bytes,recv:%u pkts %lu bytes\n",
					inet_ntoa(addr),tuple_hash->tuple.u.tcp.d_port,tuple_hash->stat.syn,tuple_hash->stat.synack,
					tuple_hash->stat.snd_p,tuple_hash->stat.snd_b,tuple_hash->stat.rcv_p,tuple_hash->stat.rcv_b);
				log_out("%s",buf);
				total_len+= len;

				list_del_init(hpos);
				free_tuple(tuple_hash);
			}
		}
		list_del_init(pos);
		free_portlist(portlist);
	}
	return total_len;
}

