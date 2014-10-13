/**
 * @file ipv4_cfg.c
 * @brief  
 * @author zhoujz, zhoujz@chinanetcenter.com
 * @version 1.0
 * @date 10/13/2014 15:57:25, 41th 星期一
 * @par Copyright:
 * Copyright (c) Chinanetcenter 2014. All rights reserved.
 */

#include <time.h>
#include "iniparser.h"
#include "clist.h"

#define PATH_CFG    "path"
#define IFMT_CFG    "log-format"
#define OFMT_CFG    "outlog-format"
#define AGRP_CFG    "auto-grp"
#define SGRP_CFG    "spc-grp"

#define HASH_SIZE   (100)

/* stat */
typedef struct _stat_info_ {
	uint32_t syn;
	uint32_t synack;
	uint32_t snd_p;
	uint32_t rcv_p;
	uint64_t snd_b;
	uint64_t rcv_b;
} st;

/* key stat */
typedef struct _keystat_ {
  char      *name;              /* key name */
  int       offset;             /* key offset from loginfo */
  int       len;                /* key len */
  list_t    *list[HASH_SIZE];   /* st item hash */
} key_st;

/* common stat item */
struct _item_ {
  void    *nextkey;       /* next key_st */
  st      st;             /* stat info */
  time_t  tm;            /* the last hit timstamp */
  void    *val;           /* this item's key value */
} st_item;

int ipv4_readcfg(char *cfgname)
{
  return 0;
}
