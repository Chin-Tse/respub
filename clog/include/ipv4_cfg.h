/**
 * @file ipv4_cfg.h
 * @brief config interface
 * @author zhoujz, zhoujz@chinanetcenter.com
 * @version 1.0
 * @date 10/14/2014 17:53:22, 41th 星期二
 * @par Copyright:
 * Copyright (c) Chinanetcenter 2014. All rights reserved.
 */

#ifndef		__IPV4_CFG_H
#define		__IPV4_CFG_H

#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <sys/time.h>
#include <signal.h>

/*For inet_addr*/
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "list.h"
#include "iniparser.h"

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

#define HASH_SIZE   (100)
#define OFMT_DEF_LEN (10)

/* Parse func-type define */
typedef void (*ipv4_parse_f)(char *, int, char*);
typedef char *(*ipv4_unparse_f)(char *);

/* config type */
typedef enum {
  KST_AGEN,
  KST_SPEC,
} kst_type_t;

/* agp config item */
typedef struct _aitem_ {
  char  *name;                      /* agp config name */
  int   nk;                         /* key num in config */
  char  **keys;                     /* keys */
  int   ns;                         /* target stat num */
  char  **stat;                     /* stat col key  */
  char  **threshold;                /* col threshold */
} acfg_item_t;

typedef struct _condition_ {
  struct list_head  list;
  char              *name;          /* stat col name */
  uint64_t          threshold;      /* threshold */
  uint32_t          offset;         /* offset in st_t */
  uint32_t          len;            /* cmp size */
  uint32_t          ref;            /* refcnt */
} cond_t;

/* stat */
typedef struct _stat_info_ {
  uint32_t syn;
	uint32_t synack;
	uint32_t txpkts;
	uint32_t rxpkts;
	uint64_t txbytes;
	uint64_t rxbytes;
} st_t;

typedef struct _st_item_ st_item;

/* key stat */
typedef struct _keystat_ {
  struct list_head    list;
  cond_t              *cond;              /* config st col & threshold */
  kst_type_t          kst_type;           /*  */
  uint32_t            opt;                /*  */
  uint32_t            action;
  char                *name;              /* key name */
  struct _keystat_    *next;              /* next key */
  uint32_t            offset;             /* key offset from loginfo */
  uint32_t            ilen;               /* key len */
  uint32_t            mask;               /* imask */
  uint32_t            olen;               /* output space */
  ipv4_unparse_f      upfunc;
  st_item             *cfgstm;            /* output space */
  uint32_t            size;
  struct hlist_head   hlist[0];   /* st item hash */
} key_st_t;

/* common stat item */
typedef struct _st_item_ {
  struct hlist_node   hn; 
  struct list_head    kst_list;       /* next key_st */
  key_st_t            *curkst;
  st_t                st;             /* stat info */
  uint32_t            opt;
  uint32_t            tm;             /* the last hit timstamp */
  uint32_t            type;             /* the last hit timstamp */
  char                data[0];        /* this item's key value */
} st_item;

/* config item */
typedef struct _cfgitem_ {
  struct list_head    list;
  char                *name;
  key_st_t            *keyst;
} cfg_t;

/* key domain */
typedef struct _kmap_ {
    char          *kname;
    uint8_t       offset;
    uint8_t       ilen;
    uint8_t       olen;
    ipv4_parse_f  parse_func;
    uint8_t       st_off;
    uint8_t       st_len;
    ipv4_unparse_f  upfunc;
    uint32_t      size;
} kattr_map_t;

/* organize key attr in order of col to speed up parse */
typedef struct _ilog_kattr_ {
  int         num;
  kattr_map_t kattr[0];
} ilog_kattr_t;

/**
 * @brief Compute the hash key for a mem region
 *
 * @param mem [in] Memory Pointer
 * @param len [in] Memory length
 *
 * @return  hash 
 */
uint32_t ipv4_cfg_hash(char *mem, int len, uint32_t size);

/**
 * @brief Load config file
 *
 * @param cfgname [in] Config file path name
 *
 * @return  0 -- success, other -- failure  
 */
dictionary *ipv4_readcfg(char *cfgname, 
    struct list_head  *cfglist,
    ilog_kattr_t      **pp_ikat);

/**
 * @brief Release config
 *
 * @param dcfg [in] config dict
 * @param cfglist [in] configitem list
 * @param pp_ikat [in] input log key attribute obj
 */
void ipv4_release_cfg(
    dictionary        *dcfg,
    struct list_head  *cfglist, 
    ilog_kattr_t      *pp_ikat);

/**
 * @brief Malloc a new st_item
 *
 * @param kst [in] special the key stat instance that this stm belongs to
 *
 * @return  Pointer to stm -- success, NULL -- failure  
 */
st_item *ipv4_cfg_stm_malloc(key_st_t *kst);

/**
 * @brief Free the Stm and its sub-kst
 *
 * @param stm [in] Pointer to stm 
 */
void ipv4_cfg_stm_free(st_item *stm);

/**
 * @brief Malloc a new st_item
 *
 * @param kst [in] special the key stat instance that this stm belongs to
 *
 * @return  Pointer to stm -- success, NULL -- failure  
 */
st_item *ipv4_stm_malloc(key_st_t *kst);

/**
 * @brief Free the Stm and its sub-kst
 *
 * @param stm [in] Pointer to stm 
 */
void ipv4_stm_free(st_item *stm);

/*  */
key_st_t *ipv4_cfg_kst_ref(key_st_t *kst);
void ipv4_cfg_kst_release(key_st_t *kst);
/**
 * @brief Malloc a new kst instance
 *
 * @return  Pointer to kst -- success, NULL -- failure  
 */
key_st_t *ipv4_kst_malloc(void);

/**
 * @brief Free kst, include it's sub-kst and instance
 *
 * @param kst [in] Pointer of kst
 */
void ipv4_kst_free(key_st_t *kst);

cond_t *ipv4_cfg_cond_get(char *stkey, char *thrd);
void ipv4_cfg_cond_free(cond_t *cond);

acfg_item_t *ipv4_cfg_aitem_get(char *aname, char *cfgval);
void ipv4_cfg_aitem_free(acfg_item_t *acfg_item);

char *ipv4_cfg_get_ifile(dictionary *dcfg);
char *ipv4_cfg_get_ofile(dictionary *dcfg);
char *ipv4_cfg_get_rfile(dictionary *dcfg);
int ipv4_cfg_get_interval(dictionary *dcfg);

/**
 * @brief Displays data as hexadecimal data
 *
 * @param buffer [in] The data buffer
 * @param length [in] the length of data to print
 * @param cols [in] The number of hexadecimal columns to display
 * @param group [in] The number of group to dixplay
 * @param prestr [in] Every row prefix string 
 */
void hexprint_buf( 
    char * buffer, 
    uint32_t  length, 
    uint32_t  cols, 
    uint32_t group,
    char *prestr);

void dump_stm(key_st_t *kst, int prefix);
void dump_key_attr_map(void);
void dump_config(struct list_head *cfglist);
void dump_ilat(ilog_kattr_t *pilat);

void test_cfg(void);

#endif		//__IPV4_CFG_H

