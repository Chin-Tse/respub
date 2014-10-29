/**
 * @file ipv4_stat.c
 * @brief Pkt stat 
 * @author zhoujz, zhoujz@chinanetcenter.com
 * @version 1.0
 * @date 10/21/2014 16:08:33, 42th 星期二
 * @par Copyright:
 * Copyright (c) Chinanetcenter 2014. All rights reserved.
 */

#include "bench.h"
#include "ipv4_cfg.h"
#include "ipv4_parse.h"
#include <pthread.h>
#include <sys/stat.h>

key_st_t *ipv4_stat_kst_match(key_st_t *kst, ilog_t *ilog);

/* 100w lines */
#define MAX_LOG_FILE_SIZE (1000000)
#define MAX_LOG_FILE_NUM  (10)

typedef struct r_log_ctrl_ {
  int       out;
  st_item   *stm;
} rlog_ctl_t;

/* config list */
struct list_head  cfg_list = LIST_HEAD_INIT(cfg_list);
/* log key map */
ilog_kattr_t      *gpilat;
/* global dict */
dictionary        *gdcfg;

FILE *in_fd = NULL;
FILE *out_fd = NULL;
FILE *result_fd = NULL;

char *in_fname = NULL;
char *out_fname = NULL;
char *result_fname = NULL;

/* Timer out tick, never be 0 */
uint32_t        timeout = 1;
pthread_mutex_t tm_mtx = PTHREAD_MUTEX_INITIALIZER;

/* seconds */
int             interval = 0;

/* only use in stat thread, don't need to protect */
uint32_t        gtimestamp = 1;


/* for performance test */
static float startTime;

static void start() {
  startTime = cpu();
}

static void stop() {
  float duration = cpu() - startTime;
  fprintf(stdout, ": \x1b[32m%.4f\x1b[0ms\n", duration);
}

static void bm(char *label, void (*fn)()) {
  printf(" %18s", label);
  fflush(stdout);
  start();
  fn();
  stop();
}

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
static inline int ipv4_stat_parse(char *buf, ilog_t *ilog, ilog_kattr_t *pia)
{
  int         index;
  char        *pch;
  kattr_map_t *kattr;
  char        *val;
  
  if (buf == NULL || ilog == NULL || pia == NULL) {
    return -EINVAL;
  }

  index = 0;
  pch = strtok(buf, ",");
  while (index < pia->num ) {
    kattr = &pia->kattr[index];
    if (!kattr->kname) {
      index++;
      pch = strtok(NULL, ",");
      continue;
    }

    val = (char *)ilog + kattr->offset;
    if (pch) {
      kattr->parse_func(val, kattr->ilen, pch);
    } else {
      /* No complete log */
      fprintf(stderr, "No complete log!\n");
      return -1;
    }
    //printf("%d, %s---%s-%x\n", index, kattr->kname, pch, *(uint32_t *)val);
    index++;
    pch = strtok(NULL, ",");
  }

  return 0;
}

key_st_t *ipv4_stat_kst_filter(key_st_t *kst, ilog_t *ilog)
{
  int       rv = 1;
  int       cmp = 1;
  char      *kmem;
  key_st_t  *kstart = NULL;
  uint32_t  tmp;


  if (!kst->cfgstm) {
    fprintf(stderr, "----Error...check cfg!----\n");
    return NULL;
  } 

  for (kstart = kst; kstart && kstart->cfgstm; kstart = kstart->next) {
    if (!kstart->action) {
      return kstart;
    }
    kmem = (char *)ilog + kstart->offset;
    if (kstart->mask) {
      tmp = *(uint32_t *)kmem & kstart->mask;
      cmp = memcmp(kstart->cfgstm->data, &tmp, kst->ilen);
      if (cmp) {
        rv = kstart->opt;
      } else {
        rv = !kstart->opt;
      }

      if (kstart->next && kstart->next->action == 0 && rv) {
        return NULL;
      }
    } else {
      cmp = memcmp(kstart->cfgstm->data, kmem, kstart->ilen);
      if (cmp) {
        rv = kstart->opt;
      } else {
        rv = !kstart->opt;
      }
      if (kstart->next && kstart->next->action == 0 && rv) {
        return NULL;
      }
    }

    if (!rv) {
      while (kstart && kstart->action) {
        kstart = kstart->next;
      }
      break;
    }
  }

  /* 
  if (kstart) {
    return ipv4_stat_kst_match(kstart, ilog);
  }*/

  return kstart;
}

st_item *ipv4_stat_kst_match(st_item *par_stm, key_st_t *kst, ilog_t *ilog) {
  int       rv, cmp;
  st_item   *stm;
  st_item   *match = NULL;
  char      *kmem;
  key_st_t  *kpos, *kn;
  uint32_t  hash;
  uint32_t  tmp;
  
  struct hlist_node *hpos;
  struct hlist_head *hlist;

  if (par_stm) {
    kst = par_stm->curkst->next;
    if (!kst) {
      return NULL;
    }
    hlist = par_stm->hlist;
  } else {
    hlist = kst->hlist;
  }
  
  kmem = (char *)ilog + kst->offset;
  if (kst->cfgstm) {
    if (kst->mask) {
      tmp = *(uint32_t *)kmem & kst->mask;
      cmp = memcmp(kst->cfgstm->data, &tmp, kst->ilen);
    } else {
      cmp = memcmp(kst->cfgstm->data, kmem, kst->ilen);
      if (!cmp) {
        match = kst->cfgstm;
      }
    }

    if (cmp) {
      if (kst->opt == 0 && kst->action == 0) {
        return NULL;
      } 
    } else {
      if (kst->opt && kst->action == 0) {
        return NULL;
      }
    }

    return kst->next;
  }

  hash = ipv4_cfg_hash(kmem, kst->ilen, kst->size);
  hlist_for_each_entry(stm, hpos, &kst->hlist[hash], hn) {
    if (!memcmp(stm->data, kmem, kst->ilen)) {
      match = stm;
      break;
    }
  }
  
  if (!match) {
    match = ipv4_cfg_stm_malloc(kst);
    if (!match) {
      return NULL;
    }
    match->tm = gtimestamp;
    memcpy(&match->data, kmem, kst->ilen);
    if (!hlist_empty(&kst->hlist[hash])) {
      hlist_add_after(kst->hlist[hash].first, &match->hn);
    } else {
      hlist_add_head(&match->hn, &kst->hlist[hash]);
      list_add_tail(&match->olist, &kst->olist);
    }
  } 

  if (match->tm == 0 && match->tm == gtimestamp) {
    match->st.syn += ilog->syn;
    match->st.synack += ilog->synack;
    match->st.txpkts += ilog->txpkts;
    match->st.rxpkts += ilog->rxpkts;
    match->st.txbytes += ilog->txbytes;
    match->st.rxbytes += ilog->rxbytes;
  } else {
    match->tm = gtimestamp;
    match->st.syn = ilog->syn;
    match->st.synack = ilog->synack;
    match->st.txpkts = ilog->txpkts;
    match->st.rxpkts = ilog->rxpkts;
    match->st.txbytes = ilog->txbytes;
    match->st.rxbytes = ilog->rxbytes;
  }

  if (list_empty(&match->kst_list)) {
    return NULL;
  } else {
    return list_first_entry(&match->kst_list, key_st_t, list);
  }

  /* next stat 
  list_for_each_entry(kpos, &match->kst_list, list) {
    ipv4_stat_kst_match(kpos, ilog);
  }*/

  return NULL;
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
  key_st_t *(*sfunc)(key_st_t *kst, ilog_t *ilog);

  if (!kst) {
    return;
  }

  while (kst) {
    if (kst->action) {
      sfunc = ipv4_stat_kst_filter;
    } else {
      sfunc = ipv4_stat_kst_match;
    }
    kst = sfunc(kst, ilog);
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

int  logtstmp = 0;

static inline void ipv4_stat_set_logtstmp()
{
  logtstmp = 1;
}

static inline int ipv4_stat_get_logtstmp(char *pbuf)
{
  int         len;
  time_t      curtm;
  struct tm   *timeinfo;
  char        buffer [80];

  len = 0;
  if (logtstmp) {
    curtm = time(NULL);
    timeinfo = localtime ( &curtm);
    strftime (buffer,80,"%Y-%m-%d-%H:%M\n",timeinfo);

    len = strlen(buffer);
    strcpy(pbuf, buffer);
    logtstmp = 0;
  }

  return len;
}

void ipv4_stat_log(rlog_ctl_t *pstm, int lv) {
  int       i;
  key_st_t  *kst;
  char      *pbuf;
  st_t      *st;
  ipv4_unparse_f func;
  char buf[1024];
  
  buf[0] = '\0';
  pbuf = buf;

  pbuf += ipv4_stat_get_logtstmp(pbuf);

  for (i = 0; i <= lv; i++) {
    /* has already output */
    if (pstm[i].out) {
      continue;
    } else {
      memset(pbuf, ' ', i << 1);
      pbuf += i << 1;

      pstm[i].out = 1;
      kst = pstm[i].stm->curkst;
      sprintf(pbuf, "%s:", kst->name);
      if (!kst->upfunc) {
        func = ipv4_parse_uint32_str;
      } else {
        func = kst->upfunc;
      }
      /* format ... */
      strcat(pbuf, func(pstm[i].stm->data));
      pbuf += strlen(pbuf);
      pbuf[0] = '\n';
      pbuf++;
    }
  }

  pbuf--;
  pbuf[0] = ',';
  pbuf++;

  st = &pstm[lv].stm->st;

  sprintf(pbuf, "rxpkts:%u, rxbytes:%lu, txpkts:%u,"
      " txbytes:%lu, syn:%u, synack:%u",
      st->rxpkts, st->rxbytes, st->txpkts, 
      st->txbytes, st->syn, st->synack);
  
  if (result_fd == NULL) {
    fprintf(stdout, "%s\n", buf);
  } else {
    fprintf(result_fd, "%s\n", buf);
  }

  return;
}

int ipv4_stat_check_aging(st_item *st)
{
#define STM_AGING_TIME   60  /* 60s aging time */
  int   ticks;

  /* config stm - never age out */
  if (st->tm == 0) {
    return 0;
  }
  //printf("----------aging----------\n");
  return 1;

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

void ipv4_stat_kst_log(key_st_t *kst, int lv, rlog_ctl_t *pstm, int pnr)
{
  int         i;
  st_item     *stm, *hst, *nst;
  cond_t      *cond;
  key_st_t    *kpos;
  key_st_t    *kstart;
  int         rv;
  static int  clv;
  
  struct hlist_node *pos, *n;
  struct hlist_head *hh;  

  if (lv > pnr) {
    return;
  }

  while (kst->cfgstm) {
    kst =  kst->next;
    if (!kst) {
      return;
    }
  }

  list_for_each_entry_safe(hst, nst, &kst->olist, olist) {
    hh = (struct hlist_head *)hst->hn.pprev;
    hlist_for_each_entry_safe(stm, pos, n, hh, hn) {
      if (stm->tm != gtimestamp) {
        /* check for aged out */
        if (ipv4_stat_check_aging(stm)) {
          ipv4_cfg_stm_free(stm, 1);
        }
        continue;
      }

      /*
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
      */
      
      rv = 1;
      if (rv) {
        pstm[lv].stm = stm;
        if (list_empty(&stm->kst_list)) {
          /* log out */
          pstm[lv].out = 0;
          ipv4_stat_log(pstm, lv);
          continue;
        }

        list_for_each_entry(kpos, &stm->kst_list, list) {
          ipv4_stat_kst_log(kpos, lv + 1, pstm, 100);
          pstm[lv].out = 0;
        }
      }
    }
  }

  return;
}

void ipv4_stat_log_out(struct list_head *cfglist)
{
  cfg_t             *cfg;
  static rlog_ctl_t pstm[100];
  
  ipv4_stat_set_logtstmp();
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
uint32_t ipv4_stat_get_tm(void)
{
  uint32_t tm;
  pthread_mutex_lock(&tm_mtx);
  tm = timeout;
  pthread_mutex_unlock(&tm_mtx);

  return tm;
}

int ipv4_stat_check_timeout(uint32_t *otime)
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
 * @brief Stat pkt
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
	int       len;
	ilog_t    info;
  uint32_t  oldtime = gtimestamp;
  FILE      *tmp_fd = NULL;
  static  FILE    *log_fd = NULL;
	static  long total_len;
	static  char  buf[1024];

  static  time_t rawtime;
  static  struct tm * timeinfo;
  static  char buffer [80];

  time ( &rawtime );
  timeinfo = localtime ( &rawtime );
  strftime (buffer,80,"%Y-%m-%d-%H:%M,",timeinfo);

  log_fd = ipv4_stat_mv_logfile(NULL);
  start();
	while (fgets(buf,1024,file) != NULL) {
		total_len += 1;
		fprintf(log_fd, "%s%s", buffer, buf);
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
      time ( &rawtime );
      timeinfo = localtime ( &rawtime );
      strftime (buffer,80,"%Y-%M-%d-%h:%m",timeinfo);
    }
	}
  stop();
  printf("total_len:%ld\n", total_len);

	ipv4_stat_log_out(cfglist);
  printf("------cfg:------\n");
  dump_config(cfglist);

  if (log_fd) {
    fclose(log_fd);
  }

  return 0;
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
  //printf("time:%u\n", timeout);

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

int ipv4_stat_create_dir(char *ofname)
{
  int   len;
  int   status;
  char  *dir;

  dir = strdup(ofname);
  len = strlen(dir);
  
  while (dir[len] != '/' ) {
    len--;
    if (len < 0) {
      return 0;
    }
  }
  dir[len] = '\0';

  status = mkdir(dir, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
  fprintf(stdout, "Create output dir:%s\n", dir);
  free(dir);

  return status;
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

  /* Get input file name */
  in_fname = ipv4_cfg_get_ifile(gdcfg);
  if (!in_fname) {
    fprintf(stderr, "Error when get input file name!\n");
    return -1;
  }
  fprintf(stdout, "Input  Logfile: %s\n", in_fname);

  /* Get output file name */
  out_fname = ipv4_cfg_get_ofile(gdcfg);
  if (!out_fname) {
    fprintf(stderr, "Error when get output file name!\n");
    return -1;
  }
  fprintf(stdout, "Output Logfile: %s\n", out_fname);
  if (!strcmp(out_fname, in_fname)) {
    fprintf(stderr, "Error, Output file is same with Input file!\n");
    return -1;
  }
  (void)ipv4_stat_create_dir(out_fname);

  /* Get result file name */
  result_fname = ipv4_cfg_get_rfile(gdcfg);
  if (!result_fname) {
    fprintf(stdout, "Result file isn't set, use standard output!\n");
  } else {
    fprintf(stdout, "Result file: %s\n", result_fname);
    if (!strcmp(result_fname, in_fname) 
        || !strcmp(result_fname, out_fname)) {
      fprintf(stderr, "Error, Result file is same with Input/Output file!\n");
      return -1;
    }

    (void)ipv4_stat_create_dir(result_fname);
    result_fd = fopen(result_fname, "wb");
    if(result_fd == NULL)	{
      fprintf(stderr,"Error when open result file:%s\n", result_fname);
      return -1;
    } 
  }

  /* input file */
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

  if (result_fd) {
    fclose(result_fd);
  }
  fclose(in_fd);
  ipv4_release_cfg(gdcfg, &cfg_list, gpilat);

  return 0;
}
