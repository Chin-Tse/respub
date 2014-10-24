/**
 * @file ipv4_cfg.c
 * @brief  
 * @author zhoujz, zhoujz@chinanetcenter.com
 * @version 1.0
 * @date 10/13/2014 15:57:25, 41th 星期一
 * @par Copyright:
 * Copyright (c) Chinanetcenter 2014. All rights reserved.
 */

#include "ipv4_cfg.h"
#include "ipv4_parse.h"
#include "str_replace.h"
#include "strsplit.h"

#define DEF_OLEN    (12)
#define CFG_FILE    "./config.txt"
#define IPATH_CFG    "path:input"
#define OPATH_CFG    "path:output"
#define RESULT_CFG   "path:result"
#define TIME_CFG     "time:interval"

#define IFMT_CFG    "log-format"
#define OFMT_CFG    "outlog-format"
#define AGRP_CFG    "auto-grp"
#define SGRP_CFG    "spc-grp"

#define KEY_OFFSET(key)  offsetof(ilog_t, key)
#define KEY_ILEN(key)    sizeof(((ilog_t*)0)->key)
#define ST_OFFSET(key)  offsetof(st_t, key)
#define ST_ILEN(key)    sizeof(((st_t*)0)->key)

/* domain cfg arry */
kattr_map_t key_attr_map[] = {
  {"srcip", KEY_OFFSET(srcip), KEY_ILEN(srcip), DEF_OLEN, ipv4_parse_ip,
  0,0, ipv4_parse_ip2str},
  {"dstip", KEY_OFFSET(dstip), KEY_ILEN(dstip), DEF_OLEN, ipv4_parse_ip,
  0,0, ipv4_parse_ip2str},
  {"sport", KEY_OFFSET(sport), KEY_ILEN(sport), DEF_OLEN, ipv4_parse_uint16, 
  0,0, ipv4_parse_uint16_str},
  {"dport", KEY_OFFSET(dport), KEY_ILEN(dport), DEF_OLEN, ipv4_parse_uint16,
  0,0, ipv4_parse_uint16_str},
  {"proto_num", KEY_OFFSET(proto_num), KEY_ILEN(proto_num), DEF_OLEN, ipv4_parse_uint8,
  0,0, ipv4_parse_uint8_str},
  {"ifname", KEY_OFFSET(ifname), KEY_ILEN(ifname), DEF_OLEN, ipv4_parse_str,
  0,0,ipv4_parse_str2str},

  {"syn", KEY_OFFSET(syn), KEY_ILEN(syn), DEF_OLEN, ipv4_parse_uint32, 
    ST_OFFSET(syn), ST_ILEN(syn)},
  {"synack", KEY_OFFSET(synack), KEY_ILEN(synack), DEF_OLEN, ipv4_parse_uint32,
    ST_OFFSET(synack), ST_ILEN(synack)},
  {"rxpkts", KEY_OFFSET(rxpkts), KEY_ILEN(rxpkts), DEF_OLEN, ipv4_parse_uint32,
    ST_OFFSET(rxpkts), ST_ILEN(rxpkts)},
  {"txpkts", KEY_OFFSET(txpkts), KEY_ILEN(txpkts), DEF_OLEN, ipv4_parse_uint32,
    ST_OFFSET(txpkts), ST_ILEN(txpkts)},
  {"rxbytes", KEY_OFFSET(rxbytes), KEY_ILEN(rxbytes), DEF_OLEN, ipv4_parse_uint64,
    ST_OFFSET(rxbytes), ST_ILEN(rxbytes), ipv4_parse_uint64_str},
  {"txbytes", KEY_OFFSET(txbytes), KEY_ILEN(txbytes), DEF_OLEN, ipv4_parse_uint64,
    ST_OFFSET(txbytes), ST_ILEN(txbytes), ipv4_parse_uint64_str},
};

/* config list 
struct list_head cfg_list = LIST_HEAD_INIT(cfg_list);
*/

/* format-cfg */
char        *infile_name;
char        *ofile_name;

kattr_map_t *ipv4_cfg_kattr_get(const char *key);

/**
 * @brief Compute the hash key for a mem region
 *
 * @param mem [in] Memory Pointer
 * @param len [in] Memory length
 *
 * @return  hash 
 */
uint32_t ipv4_cfg_hash(char *mem, int len)
{
  uint32_t    hash ;
  size_t      i ;

  for (hash=0, i=0 ; i<len ; i++) {
    hash += (unsigned)mem[i] ;
    hash += (hash<<10);
    hash ^= (hash>>6) ;
  }

  hash += (hash <<3);
  hash ^= (hash >>11);
  hash += (hash <<15);

  hash %= HASH_SIZE;

  return hash ;
}

/**
 * @brief Malloc a new st_item
 *
 * @param kst [in] special the key stat instance that this stm belongs to
 *
 * @return  Pointer to stm -- success, NULL -- failure  
 */
st_item *ipv4_cfg_stm_malloc(key_st_t *kst)
{
  st_item   *stm;
  key_st_t  *nkst;

  stm = calloc(1, sizeof(st_item) + kst->ilen);
  //stm = malloc(sizeof(st_item) + kst->ilen);
  if (!stm) {
    return NULL;
  }

  INIT_HLIST_NODE(&stm->hn);
  INIT_LIST_HEAD(&stm->kst_list);
  stm->tm = 0;
  stm->curkst = kst;
  if (kst->next) {
    /* ref list */
    nkst = ipv4_cfg_kst_ref(kst->next);
    list_add_tail(&nkst->list, &stm->kst_list);

    /* next level kst maybe a list */
    key_st_t  *nnkst, *pos;
    list_for_each_entry(pos, &kst->next->list, list) {
      nnkst = ipv4_cfg_kst_ref(pos);
      list_add_tail(&nnkst->list, &stm->kst_list);
    } 
  }

  return stm;
}

/**
 * @brief Free the Stm and its sub-kst
 *
 * @param stm [in] Pointer to stm 
 */
void ipv4_cfg_stm_free(st_item *stm)
{
  key_st_t *kst, *n;
  if (stm == NULL) {
    return;
  }

  /* remove itself from hlist */
  hlist_del(&stm->hn);

  /* del sub kst instance */
  list_for_each_entry_safe(kst, n, &stm->kst_list, list) {
    ipv4_cfg_kst_release(kst);
  }

  free(stm);

  return;
}

/**
 * @brief Dup & ref kst
 *
 * @param kst [in] kst 
 *
 * @return  Pointer -- success, NULL -- failure  
 *
 * Called when new instance generate
 */
key_st_t *ipv4_cfg_kst_ref(key_st_t *kst)
{
  key_st_t  *nkst;

  if (!kst) {
    return NULL;
  }

  nkst = malloc(sizeof(key_st_t));
  if (!nkst) {
    return NULL;
  }
  /* copy Pointer: next/cond/name, and other attr */
  memcpy(nkst, kst, sizeof(key_st_t) - sizeof(struct hlist_head) * HASH_SIZE);
  INIT_LIST_HEAD(&nkst->list);
  memset(nkst->hlist, 0, sizeof(struct hlist_head) * HASH_SIZE);

  return nkst;
}

/**
 * @brief Release kst, include it's sub-kst and instance
 *
 * @param kst [in] Pointer of kst
 *
 * Called when instance del/free
 */
void ipv4_cfg_kst_release(key_st_t *kst)
{
  int         i;
  st_item     *st;

  struct hlist_node  *pos, *n;  

  if (kst == NULL) {
    return;
  }
  /* remote from list */
  list_del(&kst->list);
  /* delete stm */
  for (i = 0; i < HASH_SIZE; i++) {
    if (hlist_empty(&kst->hlist[i])) {
      continue;
    }
    hlist_for_each_entry_safe(st, pos, n, &kst->hlist[i], hn) {
      ipv4_cfg_stm_free(st);
    }
  }

  /* only free cur kst, don't free next/name/cond thing */
  free(kst);

  return;
}

/**
 * @brief Malloc a new kst instance
 *
 * @return  Pointer to kst -- success, NULL -- failure  
 *
 * Mostly called when generate new config
 */
key_st_t *ipv4_cfg_kst_malloc(void)
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
 *
 * This func called when config item delete
 */
void ipv4_cfg_kst_free(key_st_t *kst)
{
  int         i;
  st_item     *st;
  cond_t      *cpos, *cn;
  key_st_t    *kpos, *kn;

  struct hlist_node  *pos, *n;  

  if (kst == NULL) {
    return;
  }
  
  /* only free (release not) do this */
  if (kst->next) {
    /* this domain don't free in sub-kst */
    kst->next->cond = NULL;
    ipv4_cfg_kst_free(kst->next);
  }

  if (kst->name) {
    free(kst->name);
  }

  /* sibling kst */
  list_for_each_entry_safe(kpos, kn, &kst->list, list) {
    list_del_init(&kpos->list);
    kpos->cond = NULL;
    ipv4_cfg_kst_free(kpos);
  }

  /* only free in top kst */
  if (kst->cond) {
    list_for_each_entry_safe(cpos, cn, &kst->cond->list, list) {
      ipv4_cfg_cond_free(cpos);
    }
    ipv4_cfg_cond_free(kst->cond);
  }

  /* delete stm */
  for (i = 0; i < HASH_SIZE; i++) {
    if (hlist_empty(&kst->hlist[i])) {
      continue;
    }
    hlist_for_each_entry_safe(st, pos, n, &kst->hlist[i], hn) {
      ipv4_cfg_stm_free(st);
    }
  }

  return;
}

int ipv4_cfg_kst_init(key_st_t *kst, const char *key)
{
  kattr_map_t *kattr;
  int         i, size;

  if (kst == NULL || key == NULL) {
    return -EINVAL;
  }

  kattr = ipv4_cfg_kattr_get(key);
  if (!kattr) {
    fprintf(stderr, "Error config, Key error:%s!\n", key);
    return -EINVAL;
  }
  kst->offset = kattr->offset;
  kst->ilen = kattr->ilen;
  kst->olen = kattr->olen;
  kst->upfunc = kattr->upfunc;
  kst->name = xstrdup(kattr->kname);
  if (!kst->name) {
    return -ENOMEM;
  }

  /* 
  if (val) {
    stm = ipv4_cfg_stm_malloc(kst);
    if (!smt) {
      return -ENOMEM;
    }
    kattr->parse_func(stm->data, kst->ilen, val);
    hash = ipv4_cfg_hash(stm->data, kst->ilen);
    hlist_add_head(&kst->hlist[hash])
  }
  */

  return 0;
}

st_item *ipv4_cfg_kst_stm_init(key_st_t *kst, char *key, char *val)
{
  kattr_map_t *kattr;
  uint32_t    hash;
  st_item     *stm;

  if (kst == NULL || key == NULL || val == NULL) {
    return NULL;
  }
  
  kattr = ipv4_cfg_kattr_get(key);
  if (!kattr) {
    return NULL;
  }
  if (strcmp(kst->name, key)) {
    fprintf(stderr, "Error config squence!\n");
    return NULL;
  }

  if (val) {
    stm = ipv4_cfg_stm_malloc(kst);
    if (!stm) {
      return NULL;
    }
    /* never aged */
    stm->tm = 0;
    if (val[0] == '!') {
      printf("------------no op------\n");
      kst->opt = 1;
      stm->opt = 1;
      val++;
    }
    kattr->parse_func(stm->data, kst->ilen, val);
    hash = ipv4_cfg_hash(stm->data, kst->ilen);
    kst->hash = hash;
    stm->type = KST_SPEC;
    hlist_add_head(&stm->hn, &kst->hlist[hash]);
  }
 
  return stm;
}

void ipv4_cfg_cond_free(cond_t *cond)
{
  if (!cond) {
    return;
  }

  if (--cond->ref) {
    return;
  }

  list_del(&cond->list);

  if (cond->name) {
    free(cond->name);
    cond->name = NULL;
  }

  free(cond);

  return;
}

cond_t *ipv4_cfg_cond_get(char *stkey, char *thrd)
{
  kattr_map_t *stattr;
  cond_t      *cond;

  if (stkey == NULL || thrd == NULL) {
    return NULL;
  }
  stattr = ipv4_cfg_kattr_get(stkey);
  if (!stattr) {
    fprintf(stderr, "Error config, Key error:%s!\n", stkey);
    return NULL;
  }

  cond = malloc(sizeof(cond_t));
  if (!cond) {
    return NULL;
  }
  cond->name = strdup(stkey);
  if (!cond->name) {
    goto err;
  }
  INIT_LIST_HEAD(&cond->list);
  cond->offset = stattr->st_off;
  cond->len = stattr->st_len;
  cond->threshold = (uint64_t)strtoll(thrd, NULL, 0);
  cond->ref = 1;

  return cond;
err:
  if (cond) {
    ipv4_cfg_cond_free(cond);
  }

  return NULL;
}

acfg_item_t *ipv4_cfg_aitem_get(char *aname, char *cfgval)
{
  int   i;
  int   iv;
  int   ic;
  char  *key;
  char  **ptr;
  acfg_item_t *acfg_item = NULL;

  if (aname == NULL || cfgval == NULL) {
    return NULL;
  }

  key = str_replace(cfgval, " ", "");
  if (!key) {
    return NULL;
  }

  iv = occurrences("|", key) + 1;
  ptr = malloc(sizeof(char *) * iv);
  if (!ptr) {
    goto out;
  }
  strsplit(key, ptr, "|"); 
  if (iv < 3) {
    goto out;
  }

  acfg_item = malloc(sizeof(acfg_item_t));
  if (!acfg_item) {
    goto out;
  }

  acfg_item->nk = occurrences(",", ptr[0]) + 1;
  acfg_item->keys = malloc(sizeof(char *) * acfg_item->nk);
  strsplit(ptr[0], acfg_item->keys, ",");

  acfg_item->ns = occurrences(",", ptr[1]) + 1;
  acfg_item->stat= malloc(sizeof(char *) * acfg_item->ns);
  acfg_item->threshold = malloc(sizeof(char *) * acfg_item->ns);
  strsplit(ptr[1], acfg_item->stat, ",");
  strsplit(ptr[2], acfg_item->threshold, ",");
  acfg_item->name = strdup(aname);

  if (!acfg_item->name) {
    ipv4_cfg_aitem_free(acfg_item);
    acfg_item = NULL;
  }

out:
  if (key) {
    free(key);
  }
  if (ptr) {
    for (i = 0; i < iv; i++) {
      free(ptr[i]);
    }
    free(ptr);
  }

  return acfg_item;
}

void ipv4_cfg_aitem_free(acfg_item_t *acfg_item)
{
  int i; 

  if (!acfg_item) {
    return;
  }

  if (!acfg_item->name) {
    return;
  }
  free(acfg_item->name);
  acfg_item->name =NULL;
  for (i = 0; i < acfg_item->nk; i++) {
    free(acfg_item->keys[i]);
  }
  for (i = 0; i < acfg_item->ns; i++) {
    free(acfg_item->stat[i]);
    free(acfg_item->threshold[i]);
  }

  free(acfg_item);

  return;
}

key_st_t *ipv4_cfg_aitem_add(key_st_t *tkst, acfg_item_t *acfg_item)
{
  key_st_t    *kst;
  key_st_t    *okst = NULL;
  key_st_t    *atkst = NULL;
  cond_t      *cond, *pos, *n;
  cond_t      *tcond = NULL;
  cond_t      *atcond = NULL;

  int         j;

  if (!acfg_item) {
    return NULL;
  }

  if (tkst) {
    okst = tkst;
    while ( okst->kst_type == KST_SPEC && okst->next) {
      okst = okst->next;
    }
    tcond = tkst->cond;
  }

  /* Generate KST for each agp key */
  for (j = 0; j < acfg_item->nk; j++) {
    kst = ipv4_cfg_kst_malloc();
    if (!kst) {
      goto err;
    }
    if (atkst == NULL) {
      atkst = kst;
    }
    kst->kst_type = KST_AGEN;
    if (ipv4_cfg_kst_init(kst, acfg_item->keys[j])) {
      goto err;
    }
    if (okst) {
      if (j == 0 && okst->kst_type == KST_AGEN) {
        list_add_tail(&okst->list, &kst->list);
      } else {
        okst->next = kst;
      }
    }
    okst = kst;
  }
  
  /* Generate ST_THRD_INS */
  for (j = 0; j < acfg_item->ns; j++) {
    cond = ipv4_cfg_cond_get(acfg_item->stat[j], acfg_item->threshold[j]);
    if (!cond) {
      goto err;
    }
    if (atcond == NULL) {
      atcond = cond;
      if (!tcond) {
        tcond = atcond;
      } else {
        list_add_tail(&atcond->list, &tcond->list);
      }
    } else {
      list_add_tail(&cond->list, &tcond->list);
    }
  }

  /* sub-kst get the same cond with tskt */
  kst = tkst ? tkst : atkst;
  
  while (kst) {
    kst->cond = tcond;
    kst = kst->next;
  }

  return atkst;

err:
  fprintf(stderr, "Error: add aitem!\n");
  if (atkst) {
    ipv4_cfg_kst_free(atkst);
  }
  if (atcond) {
    if (tcond != atcond) {
      pos = atcond;
      list_for_each_entry_safe_from(pos, n, &tcond->list, list) {
        ipv4_cfg_cond_free(pos);
      }
    } else {
      list_for_each_entry_safe(pos, n, &tcond->list, list) {
        ipv4_cfg_cond_free(pos);
      }
      ipv4_cfg_cond_free(tcond);
    }
  }

  return NULL;
}

/**
 * @brief Generate config item from config keys&value
 *
 * @param num [in] keys & vals number
 * @param keys [in] keys array
 * @param vals [in] vals array
 *
 * @return  key stat struct -- success, NULL -- failure  
 */
key_st_t *ipv4_cfg_item_init(int num, char **keys, char **vals, dictionary *dcfg)
{
  int         i, j;
  key_st_t    *tkst = NULL;
  key_st_t    *kst, *okst;
  dictionary  *ifmt_cfg;
  acfg_item_t *acfg_item = NULL;
  st_item     *stm = NULL;
  int         action = 0;

  /* get sub config, not need to free sub-dict */
  ifmt_cfg = iniparser_str_getsec(dcfg, IFMT_CFG);
  if (!ifmt_cfg) {
    return NULL;
  }

  /* generate config kst-list */
  okst = NULL;
  for (i = 0; i < num; i++) {
    if (iniparser_find_entry(ifmt_cfg, keys[i])) {
      kst = ipv4_cfg_kst_malloc();
      if (!kst) {
        goto err;
      }
      if (!tkst) {
        tkst = kst;
      }

      if (okst) {
        okst->next = kst;
      }
      okst = kst;

      kst->kst_type = KST_SPEC;
      if (ipv4_cfg_kst_init(kst, keys[i])) {
        goto err;
      }
      continue;
    } else {
      if (!strcmp(keys[i], "action")) {
        if (!strcmp(vals[i], "accept")) {
          action = 0;
        } else {
          action = 1;
        }
      }
      acfg_item = ipv4_cfg_aitem_get(keys[i], vals[i]);
      kst = ipv4_cfg_aitem_add(tkst, acfg_item);
      if (!tkst) {
        tkst = kst;
      }
    }
  }

  /* update kst action */
  if (action) {
    kst = tkst;
    while (kst) {
      if (kst->kst_type == KST_SPEC) {
        kst->action = action;
      }
      kst = kst->next;
    }
  }

  /* kst-list is ready, add special stm */
  kst = tkst;
  for (i = 0; i < num; i++) {
    if (iniparser_find_entry(ifmt_cfg, keys[i])) {
      if (!kst) {
        fprintf(stderr, "Check config item: %s = %s!", keys[i], vals[i]);
        break;
      }
      stm = ipv4_cfg_kst_stm_init(kst, keys[i], vals[i]);

      if (!stm) {
        fprintf(stderr, "Add config stm fail!\n");
        break;
      }
      if (!list_empty(&stm->kst_list)) {
        kst = list_first_entry(&stm->kst_list, key_st_t, list);
      } else {
        kst = NULL;
      }
    } else {
      if (strcmp(keys[i], "action")) {
        break;
      }
    }
  }

  return tkst;

err:
  if (kst) {
    ipv4_cfg_kst_free(tkst);
    kst = NULL;
  }

  return NULL;
}

/**
 * @brief Parse special auto generate group config 
 *
 * @param cfg [in] Config Item List
 * @param dcfg [in] Sub-dictionary of sepcial agp 
 *
 * @return  0 -- success, other -- failure  
 */
int ipv4_cfg_sgpcfg_gen(struct list_head *cfg, dictionary *dcfg)
{
  int         i, j;
  int         snum;
  int         knum;
  char        **keys;
  char        **vals;
  dictionary  *sdict;
  dictionary  *d;
  cfg_t       *cfgitem;

  sdict = iniparser_str_getsec(dcfg, SGRP_CFG);
  if (!sdict) {
      return -EINVAL;
  }

  snum = iniparser_getnsec(sdict);

  for (i = 0; i < snum; i++) {
    cfgitem = malloc(sizeof(cfg_t));
    cfgitem->name = xstrdup(iniparser_getsecname(sdict, i));
    knum = iniparser_getsecnkeys(sdict, cfgitem->name);
    keys = iniparser_getseckeys(sdict, cfgitem->name);
    vals = iniparser_getsecvals(sdict, cfgitem->name);

    /*  
    for (j = 0; j < knum; j++) {
      printf("knum:%d, keys[%d] %s, vals[%d] %s\n", 
          knum, j, keys[j], j, vals[j]);
    }*/

    cfgitem->keyst = ipv4_cfg_item_init(knum, keys, vals, dcfg);

    if (!cfgitem->keyst) {
      continue;
    }
    list_add_tail(&cfgitem->list, cfg);
  }

  return 0;
}

kattr_map_t *ipv4_cfg_kattr_get(const char *key)
{
  int i, size;

  size = ARRAY_SIZE(key_attr_map);

  for (i = 0; i < size; i++) {
    if (!strcmp(key, key_attr_map[i].kname)) {
      return &key_attr_map[i];
    }
  }

  return NULL;
}

/**
 * @brief Init key attr map array, return ilog_kattr array
 *
 * @param dcfg [in] config file handle
 *
 * @return  0 -- success, other -- failure  
 */
ilog_kattr_t *ipv4_cfg_kattr_init(dictionary *dcfg)
{
  ilog_kattr_t  *ilog_kattr = NULL;
  kattr_map_t   *kattr = NULL;

  dictionary    *ifmt_cfg = NULL;
  dictionary    *ofmt_cfg = NULL;

  int           i, isize, osize;
  char          **keys = NULL;
  int           col, space, max_col;

  if (dcfg == NULL) {
    return NULL;
  }

  /* get sub config, not need to free sub-dict */
  ifmt_cfg = iniparser_str_getsec(dcfg, IFMT_CFG);
  if (!ifmt_cfg) {
      goto out;
  }
  
  ofmt_cfg = iniparser_str_getsec(dcfg, OFMT_CFG);
  if (!ofmt_cfg) {
      goto out;
  }

  isize = iniparser_getsecnkeys(dcfg, IFMT_CFG);
  keys = iniparser_getseckeys(dcfg, IFMT_CFG);

  max_col = 0;
  for (i = 0; i < isize; i++) {
    col = iniparser_getint(ifmt_cfg, keys[i], -1);
    assert(col >= 0);
    if (max_col < col + 1) {
      max_col = col + 1;
    }
  }

  ilog_kattr = calloc(1, sizeof(ilog_kattr_t) + max_col * sizeof(kattr_map_t));
  ilog_kattr->num = max_col;
  for (i = 0; i < isize; i++) {
    col = iniparser_getint(ifmt_cfg, keys[i], -1);
    assert(col >= 0);

    kattr = ipv4_cfg_kattr_get(keys[i]);
    assert(kattr);
    kattr->olen = iniparser_getint(ofmt_cfg, keys[i], DEF_OLEN);
    memcpy(&ilog_kattr->kattr[col], kattr, sizeof(kattr_map_t));
  }

out:
  if (keys) {
    free(keys);
  }

  return ilog_kattr;
}

/**
 * @brief Load config file
 *
 * @param cfgname [in] Config file path name
 *
 * @return  0 -- success, other -- failure  
 */
dictionary *ipv4_readcfg(char *cfgname, 
    struct list_head  *cfglist,
    ilog_kattr_t      **pp_ikat)
{
  int           rv = 0;
  dictionary    *dcfg = NULL;
  ilog_kattr_t  *pilog_kattr = NULL;

  if (cfglist == NULL || pp_ikat == NULL) {
    return NULL;
  }

  dcfg = iniparser_load(CFG_FILE);
  if (!dcfg) {
    goto out;
  }

  pilog_kattr = ipv4_cfg_kattr_init(dcfg);
  if (!pilog_kattr) {
    goto out;
  }
  *pp_ikat = pilog_kattr;

  /* get spgcfg */
  rv = ipv4_cfg_sgpcfg_gen(cfglist, dcfg);
  if (rv) {
    goto out;
  }

  return dcfg;
  
out:
  if (dcfg) {
    iniparser_freedict(dcfg);
    dcfg = NULL;
  }

  if (pilog_kattr) {
    free(pilog_kattr);
  }

  return NULL;
}


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
    ilog_kattr_t      *pikat)
{
  if (cfglist) {
    cfg_t *cfg, *n;
    list_for_each_entry_safe(cfg, n, cfglist, list) {
      list_del_init(&cfg->list);
      ipv4_cfg_kst_free(cfg->keyst);
    }
  }

  if (pikat) {
    free(pikat);
  }
  
  if (dcfg) {
    iniparser_freedict(dcfg);
    dcfg = NULL;
  }

  return;
}

/**
 * @brief Get stat input log file path
 *
 * @param dcfg [in] config dict
 *
 * @return  0 -- path str, other -- NULL 
 */
char *ipv4_cfg_get_ifile(dictionary *dcfg)
{
  char *filename;

  filename = iniparser_getstring(dcfg, IPATH_CFG, NULL);

  return filename;
}

char *ipv4_cfg_get_ofile(dictionary *dcfg)
{
  char *filename;

  filename = iniparser_getstring(dcfg, OPATH_CFG, NULL);

  return filename;
}

char *ipv4_cfg_get_rfile(dictionary *dcfg)
{
  char *filename;

  filename = iniparser_getstring(dcfg, RESULT_CFG, NULL);

  return filename;
}

int ipv4_cfg_get_interval(dictionary *dcfg)
{
  int ti;

  ti = iniparser_getint(dcfg, TIME_CFG, 60);

  return ti;
}

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
    char *prestr)
{
  uint32_t i;

  for( i = 0; i < length; i++ ) {
    if( i % group == 0 && i != 0 ) {
      printf( " " );
    }

    if (i % cols == 0 && prestr) {
      printf("%s", prestr);
    }

    if( i < length ) {
      printf( "%02X", ( unsigned char )buffer[ i ] );
    } 

    if( i % cols < cols - 1 ) {
      printf( " " );
    } else {
      printf("\n");
    }
  }

  printf("\n");

  return;
}

/**
 * @brief dump Key attr map
 */
void dump_key_attr_map(void) 
{
  int     i, size;
  ilog_t  *ilog = NULL;

  size = ARRAY_SIZE(key_attr_map);

  for (i = 0; i < size; i++) {
    fprintf(stdout, "key:%s, offset:%d, len:%d\n", 
      key_attr_map[i].kname,key_attr_map[i].offset, key_attr_map[i].ilen);
  }

  /* test str_replace */
  char    *buf;
  buf = str_replace("a aa aaa  ", " ", "");
  fprintf(stdout, "%s\n", buf);
  free(buf);

  char *cfgname = "acfg00";
  char *cfgkey = "srcip, dport | rxpkts | 1000";
  acfg_item_t *acfg_item;
  
  acfg_item = ipv4_cfg_aitem_get(cfgname, cfgkey);
  if (!acfg_item) {
    fprintf(stdout, "Get acfg_item fail!\n");
    return;
  }

  fprintf(stdout, "%s:\n", acfg_item->name);
  fprintf(stdout, "nk:%d, ns:%d\n", acfg_item->nk, acfg_item->ns);
  for (i = 0; i < acfg_item->nk; i++) {
    fprintf(stdout, "%s\n", acfg_item->keys[i]);
  }
  for (i = 0; i < acfg_item->ns; i++) {
    fprintf(stdout, "%s, %s\n", acfg_item->stat[i], acfg_item->threshold[i]);
  }

  ipv4_cfg_aitem_free(acfg_item);
  
  return;
}

void dump_kattr(kattr_map_t *kattr)
{
  if (!kattr) {
    return;
  }

  if (!kattr->kname) {
    return;
  }
  fprintf(stdout, "key:%s, offset:%d, ilen:%d\n", 
      kattr->kname, kattr->offset, kattr->ilen);

  fprintf(stdout, "olen:%d, pf:%p, soff:%d, slen:%d, up:%p\n", 
      kattr->olen, kattr->parse_func, 
      kattr->st_off, kattr->st_len, kattr->upfunc);

  return;
}

void dump_ilat(ilog_kattr_t *pilat)
{
  int i;

  if (!pilat) {
    return;
  }

  for (i = 0; i < pilat->num; i++) {
    printf("Index:%d, total:%d\n", i, pilat->num);
    dump_kattr(&pilat->kattr[i]);
  }

  return;
}

void dump_stm(key_st_t *kst, int prefix)
{
  int         prelen;
  char        *prestr;
  int         i;
  st_item     *stm;
  st_t        *st;
  key_st_t    *kpos, *kn;  
  struct hlist_node *pos, *n;  

  if (!kst) {
    return;
  }

  prelen = 1;
  if (prefix > 0) {
    prelen = strlen("  ") * prefix + 1;
  }
  
  prestr = malloc(prelen);
  memset(prestr, ' ', prelen - 1);
  prestr[prelen - 1] = '\0';

  /* dump stm */
  for (i = 0; i < HASH_SIZE; i++) {
    if (hlist_empty(&kst->hlist[i])) {
      continue;
    }

    hlist_for_each_entry_safe(stm, pos, n, &kst->hlist[i], hn) {
      st = &stm->st;
      printf("%sstm:%s, curkst:%p, tm:%u, opt:%u, type:%d, %p\n", 
          prestr, stm->curkst->name, stm->curkst, 
          stm->tm, stm->opt, stm->type, stm);

      printf("%srxpkts:%u, txpkts:%u, syn:%u, "
          "synack:%u, rxb:%lu, txb:%lu\n", 
          prestr, st->rxpkts, st->txpkts, st->syn, 
          st->synack, st->rxbytes, st->txbytes);

      hexprint_buf(stm->data, stm->curkst->ilen, 16, 4, prestr);

      list_for_each_entry(kpos, &stm->kst_list, list) {
        dump_stm(kpos, prefix + 1);
      }
    }
  }
  
  free(prestr);

  return;
}

/**
 * @brief Dump config kst-list
 *
 * @param kst [in] Kst Pointer
 */
void dump_kst(key_st_t *kst, int prefix) 
{
  int         i;
  int         prelen;
  char        *prestr;
  cond_t      *cpos, *cn;

  if (kst == NULL) {
    return;
  }
  
  prelen = 1;
  if (prefix) {
    prelen = strlen("  ") * prefix + 1;
  } 

  prestr = malloc(prelen);
  memset(prestr, ' ', prelen);
  prestr[prelen - 1] = '\0';

  printf("%sname:%s, opt:%u,act:%d, %p\n", 
      prestr, kst->name, kst->opt, kst->action, (void*)kst);
  printf("%shs:%u, offset:%u, ilen:%u, olen:%u\n", 
      prestr, kst->hash, kst->offset, kst->ilen, kst->olen);

  /* only free in top kst */
  if (kst->cond) {
    cpos = kst->cond;
    printf("%scond:%s, cur:%p\n", 
          prestr, cpos->name, (void *)cpos);
    printf("%sthreshold:%lu\n", prestr, cpos->threshold);
    printf("%soffset:%u\n", prestr, cpos->offset);
    printf("%slen:%u\n", prestr, cpos->len);

    list_for_each_entry_safe(cpos, cn, &kst->cond->list, list) {
      printf("%scond:%s, cur:%p, next:%p\n", 
          prestr, cpos->name, (void *)cpos, (void *)cn);

      printf("%sthreshold:%lu\n", prestr, cpos->threshold);
      printf("%soffset:%u\n", prestr, cpos->offset);
      printf("%slen:%u\n", prestr, cpos->len);
    }
  }

  if (kst->next) {
    dump_kst(kst->next, prefix + 1);
  }

  free(prestr);

  return;
}

void dump_config(struct list_head *cfglist)
{
  cfg_t    *cfg;

  if (cfglist ==  NULL) {
    return;
  }

  printf("-------------\n");
  list_for_each_entry(cfg, cfglist, list) {
    dump_kst(cfg->keyst, 0);
    dump_stm(cfg->keyst, 0);
  }
  printf("-------------\n");

  return;
}

void test_cfg(void)
{
  struct list_head cfg_list = LIST_HEAD_INIT(cfg_list);
  ilog_kattr_t *pilat;
  dictionary    *dcfg;
  
  dcfg = ipv4_readcfg("./config.txt", &cfg_list, &pilat);

  if (!dcfg) {
    return;
  }

  dump_config(&cfg_list);
  ipv4_release_cfg(dcfg, &cfg_list, pilat);

  return;
}


