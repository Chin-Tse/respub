/**
 * @file ipv4_cfg.c
 * @brief  
 * @author zhoujz, zhoujz@chinanetcenter.com
 * @version 1.0
 * @date 10/13/2014 15:57:25, 41th 星期一
 * @par Copyright:
 * Copyright (c) Chinanetcenter 2014. All rights reserved.
 */

#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>

/*For inet_addr*/
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "iniparser.h"
#include "list.h"

#include "ipv4_cfg.h"

#define CFG_FILE    "./config.txt"
#define IPATH_CFG    "path:input"
#define OPATH_CFG    "path:ouput"

#define IFMT_CFG    "log-format"
#define OFMT_CFG    "outlog-format"
#define AGRP_CFG    "auto-grp"
#define SGRP_CFG    "spc-grp"

#define OFMT_DEF_LEN (10)
#define MAX_ETH_LEN (16)

#define HASH_SIZE   (100)

/* config type */
typedef enum {
  KST_AGEN,
  KST_SPEC,
} kst_type_t;

/* stat */
typedef struct _stat_info_ {
	uint32_t syn;
	uint32_t synack;
	uint32_t txpkts;
	uint32_t rxpkts;
	uint64_t txbytes;
	uint64_t rxbytes;
} st_t;

/* key stat */
typedef struct _keystat_ {
  struct list_head    list;
  char                *name;              /* key name */
  uint8_t             offset;             /* key offset from loginfo */
  uint8_t             ilen;               /* key len */
  uint8_t             olen;               /* output space */
  kst_type_t          kst_type;           /*  */
  struct hlist_head   hlist[HASH_SIZE];  /* st item hash */
} key_st_t;

/* common stat item */
struct _st_item_ {
  struct hlist_node   hn; 
  key_st_t            *nextkey;       /* next key_st */
  st_t                st;             /* stat info */
  time_t              tm;             /* the last hit timstamp */
  char                data[0];        /* this item's key value */
} st_item;

/* config item */
typedef struct _cfgitem_ {
  struct list_head    list;
  char                *name;
  key_st_t            *keyst;
} cfg_t;

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

	/*icmp code*/
	uint8_t code;
	/*for icmp original type*/
	uint8_t o_type;
	/*For icmp reply type*/
	uint8_t r_type;

  st_t    st;
} ilog_t;

typedef struct _kmap_ {
    char    *kname;
    uint8_t offset;
    uint8_t ilen;
} kattr_map_t;


#define KEY_OFFSET(key)  offsetof(ilog_t, key)
#define KEY_ILEN(key)    sizeof(((ilog_t*)0)->key)

kattr_map_t key_attr_map[] = {
  {"srcip", KEY_OFFSET(srcip), KEY_ILEN(srcip)},
  {"dstip", KEY_OFFSET(dstip), KEY_ILEN(dstip)},
  {"sport", KEY_OFFSET(sport), KEY_ILEN(sport)},
  {"dport", KEY_OFFSET(dport), KEY_ILEN(dport)},
  {"proto_num", KEY_OFFSET(proto_num), KEY_ILEN(proto_num)},
  {"ifname", KEY_OFFSET(ifname), KEY_ILEN(ifname)},
};

/* config list */
struct list_head cfg_list = LIST_HEAD_INIT(cfg_list);

/* format-cfg */
char        *infile_name;
char        *ofile_name;

dictionary  *ifmt_cfg = NULL;
dictionary  *ofmt_cfg = NULL;
dictionary  *agrp_cfg = NULL;
dictionary  *sgrp_cfg = NULL;

st_item *ipv4_stm_malloc(key_st_t *kst)
{
  st_item *stm;

  stm = calloc(1, sizeof(st_item) + kst->ilen);
  if (!stm) {
    return NULL;
  }

  INIT_HLIST_NODE(&stm->hn);

  return stm;
}

/**
 * @brief Malloc a new kst instance
 *
 * @return  Pointer to kst -- success, NULL -- failure  
 */
key_st_t *ipv4_kst_malloc(void)
{
  int       i;
  key_st_t  *kst;
  
  kst = calloc(1, sizeof(key_st_t));
  if (!kst) {
    return kst;
  }
  INIT_LIST_HEAD(&kst->list);
  for (i = 0; i < HASH_SIZE; i++) {
    INIT_HLIST_HEAD(&kst->hlist[i]);
  }

  return kst;
}

/**
 * @brief Free kst, include it's sub-kst and instance
 *
 * @param kst [in] Pointer of kst
 */
void ipv4_kst_free(key_st_t *kst)
{
  return;
}

int ipv4_cfgitem_set_keyattr(key_st_t *kst, const char *key)
{
  int rv;
  int i, size;

  if (kst == NULL || key == NULL) {
    return -EINVAL;
  }

  rv = -EINVAL;
  size = ARRAY_SIZE(key_attr_map);
  for (i = 0; i < size; i++) {
    if (!strcmp(key_attr_map[i].kname, key)) {
      kst.offset = key_attr_map[i].offset;
      kst.ilen = key_attr_map[i].ilen;
      rv = 0;
      break;
    }
  }

  return rv;
}

/**
 * @brief Generate config item from config keys&value
 *
 * @param num [in] keys & vals number
 * @param keys [in] keys array
 * @param vals [in] vals array
 * @param ifmt_cfg [in] ifmt config
 * @param ofmt_cfg [in] ofmt_config
 *
 * @return  key stat struct -- success, NULL -- failure  
 */
key_st_t *ipv4_cfgitem(
    int num,
    char **keys,
    char **vals,
    dictionary *ifmt_cfg,
    dictionary *ofmt_cfg)
{
  int       i;
  key_st_t  *tkst = NULL;
  key_st_t  *kst;

  for (i = 0; i < num; i++) {
    kst = ipv4_kst_malloc();
    if (!kst) {
      goto err;
    }
    if (!tkst) {
      tkst = kst;
    }
    if ( iniparser_find_entry(ifmt_cfg, keys[i])) {
      kst->kst_type = KST_SPEC;
      kst->olen = (uint8_t)iniparser_getint(ofmt_cfg, keys[i], OFMT_DEF_LEN);
      if (ipv4_cfgitem_set_keyattr(kst, keys[i])) {
        goto err;
      }
      /* set value */
      continue;
    }
  }

err:
  if (kst) {
    ipv4_kst_free(tkst);
    kst = NULL;
  }

  return NULL;
}

/**
 * @brief Parse special auto generate group config 
 *
 * @param cfg [in] Config Item List
 * @param sdict [in] Sub-dictionary of sepcial agp 
 * @param ifmt_cfg [in] Sub-dictionary of log format config
 * @param ofmt_cfg [in] Sub-dictionary of output format config
 *
 * @return  0 -- success, other -- failure  
 */
int ipv4_gen_sgpcfg(
    struct list_head *cfg,
    dictionary *sdict, 
    dictionary *ifmt_cfg, 
    dictionary *ofmt_cfg )
{
  int         i, j;
  int         snum;
  int         knum;
  char        **keys;
  char        **vals;
  dictionary  *d;
  cfg_t       *cfgitem;

  snum = iniparser_getnsec(sdict);

  for (i = 0; i < snum; i++) {
    cfgitem = malloc(sizeof(cfg_t));
    cfgitem->name = xstrdup(iniparser_getsecname(sdict, i));
    knum = iniparser_getsecnkeys(sdict, cfgitem->name);
    keys = iniparser_getseckeys(sdict, cfgitem->name);
    vals = iniparser_getsecvals(sdict, cfgitem->name);
    cfgitem->keyst = ipv4_cfg_item(knum, keys, vals, ifmt_cfg, ofmt_cfg);
  }

  return 0;
}


/**
 * @brief Load config file
 *
 * @param cfgname [in] Config file path name
 *
 * @return  0 -- success, other -- failure  
 */
int ipv4_readcfg(char *cfgname)
{
  int         rv = 0;
  dictionary  *dcfg;

  dcfg = iniparser_load(CFG_FILE);
  if (!dcfg) {
    rv = -ENOENT;
    goto out;
  }

  infile_name = iniparser_getstring(dcfg, IPATH_CFG, NULL);
  if (!infile_name) {
    rv = -EINVAL;
    goto out;
  }

  ofile_name = iniparser_getstring(dcfg, OPATH_CFG, NULL);
  if (!infile_name) {
    rv = -EINVAL;
    goto out;
  }

  /* get sub config, not need to free sub-dict */
  ifmt_cfg = iniparser_str_getsec(dcfg, IFMT_CFG);
  if (!ifmt_cfg) {
      rv = -EINVAL;
      goto out;
  }

  ofmt_cfg = iniparser_str_getsec(dcfg, OFMT_CFG);
  if (!ofmt_cfg) {
      rv = -EINVAL;
      goto out;
  }

  agrp_cfg = iniparser_str_getsec(dcfg, AGRP_CFG);
  if (!agrp_cfg) {
      rv = -EINVAL;
      goto out;
  }

  sgrp_cfg = iniparser_str_getsec(dcfg, SGRP_CFG);
  if (!sgrp_cfg) {
      rv = -EINVAL;
      goto out;
  }

out:
  if (dcfg) {
    iniparser_freedict(dcfg);
  }

  return rv;
}


/**
 * @brief dump Key attr map
 */
void dump_key_attr_map(void) {
  int i, size;

  size = ARRAY_SIZE(key_attr_map);

  for (i = 0; i < size; i++) {
    fprintf(stdout, "key:%s, offset:%d, len:%d\n", 
      key_attr_map[i].kname,key_attr_map[i].offset, key_attr_map[i].ilen);
  }

  return;
}

