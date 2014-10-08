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


int icmp_match(struct log_info *info);
int icmp_stat(struct log_info *info);
int icmp_tuple_stat(struct port_list *portlist,struct ipv4_tuple *tuple,struct stat_info *stat);
uint32_t icmp_time_out(struct list_head *list );

struct ipv4_log_stat_s icmp_log_stat = {
	.protocol = IPPROTO_ICMP,
	.name = "icmp",
	.log_match = icmp_match,
	.log_stat = icmp_stat,
	.time_out = icmp_time_out,
	.tuple_stat = icmp_tuple_stat,
};


/*For ICMP,there is not port,we keep one port list for nomal*/

/*Add the match here*/
int icmp_match(struct log_info *info)
{
	return LOG_ACCEPT;
}
/*For icmp,we keep one portlist*/
int icmp_stat(struct log_info *info)
{
	struct ipv4_tuple tuple;
	struct list_head *pos;
	struct port_list *portlist = NULL;

	/*Create tuple*/
	tuple.s_ip = info->src;
	tuple.d_ip = info->dst;
	tuple.u.icmp.code = info->code;
	tuple.u.icmp.o_type = info->o_type;
	tuple.u.icmp.r_type = info->r_type;

	/*Find tuple: list is empty,create it */
	if(list_empty(&icmp_log_stat.list))
	{
		/*Alloc new port list,and set head*/
		portlist = calloc_portlist();
		if(NULL == portlist)
			return -1;
		/*set it to be head*/
		list_add_tail(&portlist->entry,&icmp_log_stat.list);
		INIT_LIST_HEAD(&portlist->list);
		if(icmp_log_stat.tuple_stat(portlist,&tuple,&info->stat) < 0)
		{
			free_portlist(portlist);
			return -1;
		}
	//	INIT_LIST_HEAD(&portlist->entry);
	//	icmp_log_stat.list = portlist;
		/*Stat totol:*/
		portlist->stat = info->stat;
		portlist->port = info->dport;
		portlist->conn_num = 1;
		
	}
	list_for_each(pos,&icmp_log_stat.list)
	{
		/*list is exist,check if the tuple is already exist*/
		portlist = list_entry(pos,struct port_list,entry);
		/*stat tuple and return*/
		if(icmp_log_stat.tuple_stat(portlist,&tuple,&info->stat) < 0)
		{
			return -1;
		}
		/*Stat totol:*/
		portlist->stat.snd_b += info->stat.snd_b;
		portlist->stat.snd_p += info->stat.snd_p;
		portlist->stat.rcv_b += info->stat.rcv_b;
		portlist->stat.rcv_p += info->stat.rcv_p;
		portlist->conn_num++;
	}
	return 0;	
}

int icmp_tuple_stat(struct port_list *portlist,struct ipv4_tuple *tuple,struct stat_info *stat)
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
		list_add_tail(&tuple_hash->entry,&portlist->list);
		//INIT_LIST_HEAD(&(portlist->tuple_hash->entry));
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
		if((tuple_hash->tuple.d_ip == tuple->d_ip) &&
			(tuple_hash->tuple.u.icmp.code == tuple->u.icmp.code) &&
			(tuple_hash->tuple.u.icmp.o_type == tuple->u.icmp.o_type) &&
			(tuple_hash->tuple.u.icmp.r_type == tuple->u.icmp.r_type))
		{
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

uint32_t icmp_time_out(struct list_head *list)
{
	struct list_head *pos,*n;
	struct list_head *hpos,*hn;
	struct ipv4_tuple_hash *tuple_hash = NULL;
	struct port_list *portlist;
	struct in_addr addr;
	uint32_t len,tottal_len = 0;
	char buf[1024] = {'\0'};
	
	if(list_empty(list))
		return 0;

	list_for_each_safe(pos,n,list)
	{
		portlist = list_entry(pos,struct port_list,entry);

		//if(portlist->tuple_hash != NULL)
		if(!list_empty(&(portlist->list)))
		{
			len = snprintf(buf,1024,"ICMP: conn_num:%u, send:%u pkts %lu bytes,recv:%u pkts %lu bytes\n",
					portlist->conn_num,portlist->stat.snd_p,portlist->stat.snd_b,
					portlist->stat.rcv_p,portlist->stat.rcv_p);
			tottal_len += len;
			log_out("%s",buf);
			list_for_each_safe(hpos,hn,&(portlist->list))
			{
				tuple_hash = list_entry(hpos,struct ipv4_tuple_hash,entry);
				//addr.sin_addr = tuple_hash->tuple.d_ip;
				memcpy(&addr,&(tuple_hash->tuple.d_ip),4);
				len = snprintf(buf,1024,"\t%s, code:%d, ori_type:%d, reply_type:%d, send:%u pkts %lu bytes,recv:%u pkts %lu bytes\n",
						inet_ntoa(addr),tuple_hash->tuple.u.icmp.code,tuple_hash->tuple.u.icmp.o_type,
						tuple_hash->tuple.u.icmp.r_type,tuple_hash->stat.snd_p,tuple_hash->stat.snd_b,
						tuple_hash->stat.rcv_p,tuple_hash->stat.rcv_b);
				tottal_len += len;
				log_out("%s",buf);
				list_del_init(hpos);
				free_tuple(tuple_hash);
			}
		}
		list_del_init(&portlist->entry);
		free_portlist(portlist);
	}
	return tottal_len;
}




