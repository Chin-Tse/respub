/**
 * @file ipv4_parse.h
 * @brief Parse function 
 * @author zhoujz, zhoujz@chinanetcenter.com
 * @version 1.0
 * @date 10/16/2014 10:12:41, 41th 星期四
 * @par Copyright:
 * Copyright (c) Chinanetcenter 2014. All rights reserved.
 */

#ifndef		__IPV4_PARSE_H
#define		__IPV4_PARSE_H

#define MAX_ETH_LEN (16)

/* input log info */
typedef struct _in_log_{
  /* interface name */
	char    ifname[MAX_ETH_LEN];

	/*flow info*/
	uint32_t srcip;
	uint32_t dstip;
	uint16_t sport;
	uint16_t dport;

	/* Protocol num */
	uint8_t proto_num;

  uint32_t syn;
	uint32_t synack;
	uint32_t txpkts;
	uint32_t rxpkts;
	uint64_t txbytes;
	uint64_t rxbytes;

	/*icmp code*/
	uint8_t code;
	/*for icmp original type*/
	uint8_t o_type;
	/*For icmp reply type*/
	uint8_t r_type;

} ilog_t;

/* Parse func-type define */
typedef void (*ipv4_parse_f)(char *, int, char*);

/**
 * @brief parse iptype string to mem with uint32_t format
 *
 * @param mem [io] Pointer to memory
 * @param len [in] mem size
 * @param val [in] iptype string
 */
void ipv4_parse_ip(char *mem, int len, char *val);

/**
 * @brief Parse string type: copy str to mem
 *
 * @param mem [io] Pointer to memory
 * @param len [in] mem size 
 * @param val [in] string
 */
void ipv4_parse_str(char *mem, int len, char *val);

void ipv4_parse_uint8(char *mem, int len, char *val);
void ipv4_parse_uint16(char *mem, int len, char *val);
void ipv4_parse_uint32(char *mem, int len, char *val);
void ipv4_parse_uint64(char *mem, int len, char *val);

#endif		//__IPV4_PARSE_H

