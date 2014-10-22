/**
 * @file ipv4_stat.c
 * @brief Pkt stat 
 * @author zhoujz, zhoujz@chinanetcenter.com
 * @version 1.0
 * @date 10/21/2014 16:08:33, 42th 星期二
 * @par Copyright:
 * Copyright (c) Chinanetcenter 2014. All rights reserved.
 */

#include "ipv4_cfg.h"
#include "ipv4_parse.h"
#include <pthread.h>

/* 100w lines */
#define MAX_LOG_FILE_SIZE (1000000)
#define MAX_LOG_FILE_NUM  (10)

/* config list */
struct list_head  cfg_list = LIST_HEAD_INIT(cfg_list);
/* log key map */
ilog_kattr_t      *gpilat;
/* global dict */
dictionary        *gdcfg;

FILE *in_fd = NULL;
FILE *out_fd = NULL;

char *in_fname = NULL;
char *out_fname = NULL;

/* Timer out tick, never be 0 */
int             timeout = 1;
pthread_mutex_t tm_mtx = PTHREAD_MUTEX_INITIALIZER;

/* seconds */
int             interval = 0;

/* only use in stat thread, don't need to protect */
int             gtimestamp = 1;

/**
 * @brief Get a new output fd and mng all cache file
 *
 * @param log_fd [in] the output fd this time
 *
 * @return  FD -- success, NULL -- failure  
 */
FILE *ipv4_stat_mv_logfile(FILE *log_fd)
{
  static int  cnt = 0;
  static char fname[128];
  static char nfname[128];
  FILE        *tmp_file = NULL;
  int         i;
  int         len = 0;

  if (out_fname == NULL) {
    return NULL;
  }

  /* remove oldest file */
  if(cnt == MAX_LOG_FILE_NUM) {
    len = snprintf(fname, 128,"%s.%d", out_fname, cnt);
    fname[len] = '\0';
    remove(fname);
    cnt--;
  }

  /* rename cache file */
  for (i = cnt; i >= 0; i--) {
    len = snprintf(fname,128,"%s.%d", out_fname, i);
    fname[len] = '\0';
    len = snprintf(nfname,128,"%s.%d", out_fname,i+1);
    nfname[len] = '\0';
    printf("%s -> %s\n", fname, nfname);
    rename(fname, nfname);
  }

  /* reopen  */
  snprintf(fname, 128, "%s.0", out_fname);
  tmp_file = fopen(fname, "wb");
  if(tmp_file == NULL) {
    printf("exit 1\n");
    return NULL;
  }
  if (log_fd) {
    fclose(log_fd);
  }
  cnt++;

  return tmp_file;
}

/**
 * @brief Parse log-line into ilog_t
 *
 * @param buf [in] Log-line
 * @param ilog [in] Ilog_t pointer
 * @param pia [in] Line format attribute
 *
 * @return  0 -- success, other -- failure  
 */
int ipv4_stat_parse(char *buf, ilog_t *ilog, ilog_kattr_t *pia)
{
  int         index;
  char        *pch;
  kattr_map_t *kattr;
  char        *val;
  
  if (buf == NULL || ilog == NULL || pia == NULL) {
    return -EINVAL;
  }

  pch = strtok(buf, ",");
  while (index < pia->num ) {
    kattr = &pia->kattr[index];
    if (!kattr->kname) {
      index++;
      pch = strtok(NULL, ",");
      continue;
    }
    //printf("%d, %s---%s\n", index, kattr->kname, pch);

    val = (char *)ilog + kattr->offset;
    if (pch) {
      kattr->parse_func(val, kattr->ilen, pch);
    } else {
      /* No complete log */
      return -1;
    }
    index++;
    pch = strtok(NULL, ",");
  }

  return 0;
}

/**
 * @brief Stat pkts of each kst
 *
 * @param kst [in] kst
 * @param ilog [in] input log 
 *
 */
void ipv4_stat_kst(key_st_t *kst, ilog_t *ilog)
{
  /* walk item to stat */
  st_item   *stm;
  st_item   *match = NULL;
  char      *kmem;
  key_st_t  *kpos, *kn;
  uint32_t  hash;
  
  struct hlist_node *hpos;

  if (!kst) {
    return;
  }

  kmem = (char *)ilog + kst->offset;
  hash = ipv4_cfg_hash(kmem, kst->ilen);
  hlist_for_each_entry(stm, hpos, &kst->hlist[hash], hn) {
    if (!memcmp(stm->data, kmem, kst->ilen)) {
      match = stm;
      break;
    }
  }

  if (!match) {
    /* config specify the key & keyval */
    if (kst->kst_type == KST_SPEC) {
      return;
    }

    match = ipv4_cfg_stm_malloc(kst);
    if (!match) {
      return;
    }
    memcpy(&match->data, kmem, kst->ilen);
    hlist_add_head(&match->hn, &kst->hlist[hash]);
  }
  if (match->tm == gtimestamp) {
    match->st.syn += ilog->syn;
    match->st.synack += ilog->synack;
    match->st.txpkts += ilog->txpkts;
    match->st.rxpkts += ilog->rxpkts;
    match->st.txbytes += ilog->txbytes;
    match->st.rxbytes += ilog->rxbytes;
  } else {
    match->st.syn = ilog->syn;
    match->st.synack = ilog->synack;
    match->st.txpkts = ilog->txpkts;
    match->st.rxpkts = ilog->rxpkts;
    match->st.txbytes = ilog->txbytes;
    match->st.rxbytes = ilog->rxbytes;
    match->tm = gtimestamp;
  }

  /* next stat */
  list_for_each_entry(kpos, &match->kst_list, list) {
    ipv4_stat_kst(kpos, ilog);
  }

  return;
}

static inline int ipv4_stat_stm_chkcond(st_item *stm, cond_t *cond)
{
  uint32_t  *val32;
  uint64_t  *val64;
  cond_t    *pcond;

  if (cond->len > 4) {
    val64 = (uint64_t *)((char *)(&stm->st) + cond->offset);
    return *val64 >= cond->threshold;
  } else {
    val32 = (uint32_t *)((char *)(&stm->st) + cond->offset);
    return *val32 >= cond->threshold;
  }

  return 0;
}

#define KEY_OFFSET(key)  offsetof(ilog_t, key)
#define KEY_ILEN(key)    sizeof(((ilog_t*)0)->key)

void ipv4_stat_log(st_item **pstm, int lv) {
  int       i;
  key_st_t  *kst;
  char      buf[1024];
  char      *pbuf;
  st_t      *st;
  ipv4_unparse_f func;
  
  buf[0] = '\0';
  pbuf = buf;

  for (i = 0; i <= lv; i++) {
    kst = pstm[i]->curkst;
    sprintf(pbuf, "%s:", kst->name);
    if (!kst->upfunc) {
      func = ipv4_parse_uint32_str;
    } else {
      func = kst->upfunc;
    }
    /* format ... */
    strcat(pbuf, func(pstm[i]->data));
    pbuf += strlen(pbuf);
    pbuf[0] = ',';
    pbuf++;
  }
  st = &pstm[lv]->st;

  sprintf(pbuf, "rxpkts:%u, rxbytes:%lu, txpkts:%u,"
      " txbytes:%lu, syn:%u, synack:%u",
      st->rxpkts, st->rxbytes, st->txpkts, 
      st->txbytes, st->syn, st->synack);
  
  fprintf(stdout, "%s\n", buf);

  return;
}

int ipv4_stat_check_aging(st_item *st)
{
#define STM_AGING_TIME   60  /* 60s aging time */
  int   ticks;

  if (gtimestamp > st->tm) {
    ticks = gtimestamp - st->tm  - 1;
    if (ticks * interval > STM_AGING_TIME) {
      return 1;
    }
  } else {
    return 1;
  }

  return 0;
}

void ipv4_stat_kst_log(key_st_t *kst, int lv, st_item **pstm, int pnr)
{
  int       i;
  st_item   *stm;
  cond_t    *cond;
  key_st_t  *kpos;
  int       rv;
  
  struct hlist_node *pos, *n;

  if (lv > pnr) {
    return;
  }

  for (i = 0; i < HASH_SIZE; i++) {
    if (hlist_empty(&kst->hlist[i])) {
      continue;
    }

    hlist_for_each_entry_safe(stm, pos, n, &kst->hlist[i], hn) {
      if (stm->tm != gtimestamp) {
        /* check for aged out */
        if (ipv4_stat_check_aging(stm)) {
          ipv4_cfg_stm_free(stm);
        }
        continue;
      }
      if (!kst->cond) {
        continue;
      }
      rv = ipv4_stat_stm_chkcond(stm, kst->cond);
      if (!rv) {
        list_for_each_entry(cond, &kst->cond->list, list) {
          rv = ipv4_stat_stm_chkcond(stm, cond);
          if (rv) {
            break;
          }
        }
      }
      
      if (rv) {
        pstm[lv] = stm;
        if (list_empty(&stm->kst_list)) {
          /* log out */
          ipv4_stat_log(pstm, lv);
          return;
        }
        list_for_each_entry(kpos, &stm->kst_list, list) {
          ipv4_stat_kst_log(kpos, lv + 1, pstm, 100);
        }
      }
    }
  }

  return;
}

void ipv4_stat_log_out(struct list_head *cfglist)
{
  cfg_t           *cfg;
  static st_item  *pstm[100];
  
  list_for_each_entry(cfg, cfglist, list) {
    ipv4_stat_kst_log(cfg->keyst, 0, pstm, 100);
  }

  return;
}

/**
 * @brief Get current timestamp
 *
 * @return  current timestamp  
 */
int ipv4_stat_get_tm(void)
{
  int tm;
  pthread_mutex_lock(&tm_mtx);
  tm = timeout;
  pthread_mutex_unlock(&tm_mtx);

  return tm;
}

int ipv4_stat_check_timeout(int *otime)
{
  int rv;

  if ( timeout != *otime ) {
    pthread_mutex_lock(&tm_mtx);
    *otime = timeout;
    pthread_mutex_unlock(&tm_mtx);
    return 1;
  }

  return 0;
}

/**
 * @brief Stat pkts
 *
 * @param file [in] input logfile
 * @param pilat [in] key parse attribute
 * @param cfglist [in] config item list
 *
 * @return  0 -- success, other -- failure  
 */
int ipv4_stat(
    FILE              *file,
    ilog_kattr_t      *pilat,
    struct list_head  *cfglist)
{
	int     len;
	ilog_t  info;
  int     oldtime;
  FILE    *tmp_fd = NULL;
  static  FILE    *log_fd = NULL;
	static  long total_len;
	static  char  buf[1024];

  log_fd = ipv4_stat_mv_logfile(NULL);
	while (fgets(buf,1024,file) != NULL) {
		total_len += 1;
		fprintf(log_fd, "%s", buf);
		if(total_len >= MAX_LOG_FILE_SIZE) {
			tmp_fd = ipv4_stat_mv_logfile(log_fd);
			if (tmp_fd) {
				total_len=0;
        log_fd = tmp_fd;
      }
		}

		if(ipv4_stat_parse(buf, &info, pilat) == 0) {
      cfg_t   *cfg;
      list_for_each_entry(cfg, cfglist, list) {
        ipv4_stat_kst(cfg->keyst, &info);
      }
		}
    /* don't get time every line */
		if(ipv4_stat_check_timeout(&oldtime)) {
      /* output first */
			ipv4_stat_log_out(cfglist);
      gtimestamp = oldtime;
		}
	}

	ipv4_stat_log_out(cfglist);

  if (log_fd) {
    fclose(log_fd);
  }

  return;
}

void ipv4_stat_timeout_hdl(int signo)
{
  pthread_mutex_lock(&tm_mtx);
  timeout++;
  /* loopback */
  if (timeout == 0) {
    timeout++;
  }
  pthread_mutex_unlock(&tm_mtx);

  return;
}

/*  init */
void ipv4_stat_init_time(int interval)
{
  struct sigaction act;
  struct itimerval val;

  act.sa_handler = ipv4_stat_timeout_hdl;
  act.sa_flags   = 0;
  sigemptyset(&act.sa_mask);
  sigaction(SIGALRM, &act, NULL);

  val.it_value.tv_sec = interval;
  val.it_value.tv_usec = 0;
  val.it_interval = val.it_value;
  setitimer(ITIMER_REAL, &val, NULL);

  return;
}

int main(int argc,char* argv[])
{
  gdcfg = ipv4_readcfg("./config.txt", &cfg_list, &gpilat);

  if (!gdcfg) {
    fprintf(stderr, "Error in read config file!\n");
    return -1;
  }

  dump_config(&cfg_list);
  //dump_ilat(gpilat);

  in_fname = ipv4_cfg_get_ifile(gdcfg);
  if (!in_fname) {
    fprintf(stderr, "Error when get input file name!\n");
    return -1;
  }
  fprintf(stdout, "Input  Logfile: %s\n", in_fname);

  out_fname = ipv4_cfg_get_ofile(gdcfg);
  if (!out_fname) {
    fprintf(stderr, "Error when get output file name!\n");
    return -1;
  }
  fprintf(stdout, "Output Logfile: %s\n", out_fname);

  in_fd = fopen(in_fname, "rb");
  if(in_fd == NULL)	{
    fprintf(stderr,"Error when open input file:%s\n", in_fname);
    return -1;
  }

  /* start timer */
  interval = ipv4_cfg_get_interval(gdcfg);
  fprintf(stdout, "Time interval:%d\n", interval);
  ipv4_stat_init_time(interval);

  /* start stat */
  ipv4_stat(in_fd, gpilat, &cfg_list);

  fclose(in_fd);
  ipv4_release_cfg(gdcfg, &cfg_list, gpilat);

  return 0;
}
