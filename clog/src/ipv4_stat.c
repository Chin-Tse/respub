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

/* 100w lines */
#define MAX_LOG_FILE_SIZE (1000000)
#define MAX_LOG_FILE_NUM  (10)

/* config list */
struct list_head  cfg_list = LIST_HEAD_INIT(cfg_list);
/* log key map */
ilog_kattr_t      *pilat;
/* global dict */
dictionary        *gdcfg;

FILE *in_fd = NULL;
FILE *out_fd = NULL;

char *in_fname = NULL;
char *out_fname = NULL;

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
  int         i;
  char        cmd[128];
  int         len = 0;

  if (out_fname == NULL) {
    return NULL;
  }

  /* remove oldest file */
  if(cnt == MAX_LOG_FILE_NUM) {
    len = snprintf(cmd, 128,"rm -f %s.%d ", out_fname, cnt);
    cmd[len] = '\0';
    system(cmd);
    cnt--;
  }

  /* rename cache file */
  for (i = cnt;i >= 0;i--) {
    len = snprintf(cmd,128,"mv %s.%d %s.%d ", out_fname,i, out_fname,i+1);
    cmd[len] = '\0';
    system(cmd);
  }

  /* reopen  */
  char tmp[64] = {'\0'};
  FILE *tmp_file = NULL;
  snprintf(tmp,64,"%s.0",out_fname);
  tmp_file = fopen(tmp,"wb");
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

  index = 0;
  pch = strtok(buf, ",");
  while (index < pia->num && pch) {
    kattr = &pia->kattr[index];
    if (!kattr) {
      index++;
      pch = strtok(NULL, ",");
      continue;
    }
    val = (char *)ilog + kattr->offset;
    kattr->parse_func(val, kattr->ilen, pch);
  }

  return 0;
}

/**
 * @brief Get current timestamp
 *
 * @return  current timestamp  
 */
time_t ipv4_stat_get_tm(void)
{
  return 1;
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
    match = ipv4_cfg_stm_malloc(kst);
    if (!match) {
      return;
    }
    hlist_add_head(&match->hn, &kst->hlist[hash]);
  }
  match->st.syn += ilog->syn;
  match->st.synack += ilog->synack;
  match->st.txpkts += ilog->txpkts;
  match->st.rxpkts += ilog->rxpkts;
  match->st.txbytes += ilog->txbytes;
  match->st.rxbytes += ilog->rxbytes;

  /* next stat */
  list_for_each_entry(kpos, &match->kst_list, list) {
    ipv4_stat_kst(kst, ilog);
  }

  return;
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
	char    *buf;
	int     len;
	ilog_t  info;
  FILE    *log_fd = NULL;
  FILE    *tmp_file = NULL;
	static  long total_len;

  buf = malloc(1024);
  if (!buf) {
    fprintf(stderr, "Error:Can't malloc memory!\n");
  }

  log_fd = ipv4_stat_mv_logfile(NULL);
	while (fgets(buf,1024,file) != NULL) {
		total_len += 1;
		fprintf(log_fd,"%s",buf);

		if(total_len >= MAX_LOG_FILE_SIZE) {
			if (ipv4_stat_mv_logfile(log_fd)) {
				total_len=0;
      }
		}

		if(ipv4_stat_parse(buf, &info, pilat) == 0) {
      cfg_t   *cfg;
      list_for_each_entry(cfg, cfglist, list) {
        ipv4_stat_kst(cfg->keyst, &info);
      }
		}

    /* don't get time every line */
		if(0 == ipv4_log_check_time()) {
			ipv4_log_time_out();
		}
	}

  return;
}

int main(int argc,char* argv[])
{
  return 0;
}
