import pandas as pd
import numpy as np
import configobj
import time
import threading

import sys
import types

import IPy
import stat_cfg

from pandas import Series, DataFrame
from configobj import ConfigObj
from stat_cfg import auto_cfg, spc_gcfg, acfg_entry
from IPy import IP, IPSet

if sys.version_info >= (3,):
    INT_TYPES = (int,)
    STR_TYPES = (str,)
    xrange = range
else:
    INT_TYPES = (int, long)
    STR_TYPES = (str, unicode)

# stat parameters
class stat_arg(object):
  def __init__(self):
    self.pline = 0;
    self.cfgfile = './config.txt'
    self.config = stat_cfg.getcfg(self.cfgfile);
    self.cfg_fmt = self.config['log-format']
    self.cfg_agp = self.config['auto-grp'], 
    self.cfg_sgp = self.config['spc-grp']
    self.logfile = self.config['path']['input']
    print self.logfile
    self.scfg = spc_gcfg(self.cfg_sgp, self.cfg_fmt)
    self.max_col = 0
    self.max_col = max(int(col) for col in self.cfg_fmt.values()) 
    self.max_col += 1
    self.cfg_fmt['src_net'] = self.max_col
    self.max_col += 1
    self.cfg_fmt['dst_net'] = self.max_col
 
# process subnet thing
def stat_prepare(data, args, spc_idx):
  # conver with user cfg
  args.scfg.set_spc_matchlist(spc_idx, args.scfg.get_spc_dmlist(spc_idx))
  dmlist = args.scfg.get_spc_matchlist(spc_idx)
  match = args.scfg.get_spc_dmmatch(spc_idx)
  cfg_fmt = args.cfg_fmt

  dlen = len(dmlist)
  mlen = len(match)
  if dlen != mlen:
    raise ValueError("Config Error:", dmlist, match)

  if len(data) < 1:
    print "No new data for process!"
    return None

  cmpstr = "True"
  for idx, dm, mt in zip(range(dlen), dmlist, match):
    print '--------', dmlist[idx], cfg_fmt['srcip'], cfg_fmt['dstip']
    if dm == int(cfg_fmt['srcip']):
      dmlist[idx] = int(cfg_fmt['src_net'])
      try:
        prefix = IP(mt).prefixlen()
        data[dmlist[idx]] = data[dm].map(lambda x: str(IP(x).make_net(prefix)))
        mt = str(IP(mt))
        dm = dmlist[idx]
      except:
        raise ValueError("Error srcip config:", mt)
      
    elif dm == int(cfg_fmt['dstip']):
      dmlist[idx] = int(cfg_fmt['dst_net'])
      try:
        prefix = IP(mt).prefixlen()
        data[dmlist[idx]] = data[dm].map(lambda x: str(IP(x).make_net(prefix)))
        mt = str(IP(mt))
        dm = dmlist[idx]
      except:
        raise ValueError("Error srcip config:", mt)

    # origanize cmp string
    # target cmp string should be like 
    if isinstance(data.iat[0, dm], STR_TYPES):
      mtstr = "'" + str(mt) + "'" 
    else:
      mtstr = str(mt)

    strtmp = " & (" + "data[" + str(dm) + "] == " + mtstr + ")" 
    cmpstr = cmpstr + strtmp

  # filter out target data
  print cmpstr
  return data[eval(cmpstr)]
  
def stat_grp(df, agcfg_item):
  for dm in colist:
    gp = df.groupby(dm)
    for gkey, mgrp in gp:
      val_list = list(gp.sum().loc(gkey, olist))
      dbgstr = str(gkey) + str(val_list)
    
  pass

def stat_print(data, olist, thd_list):
  pass
   
def stat_tm_action(args=[]):
  print 'timeout:'
  #arg = args[0]
  global t        #Notice: use global variable!
  t=threading.Timer(5, stat_tm_action, [args])
  t.start()

  # read data
  #data = pd.read_csv(args.logfile, header=None, nrows=100, parse_dates=[0], skiprows=args.pline)
  print args.logfile
  data = pd.read_csv(args.logfile, header=None, nrows=100, parse_dates=[0], skiprows=args.pline)
  

  slen = args.scfg.get_spcfg_num()
  print "slen:", slen 
  for sidx in range(slen):
    dtmp = stat_prepare(data, args, sidx)
    for aidx in range(args.scfg.get_spcagc_num(sidx)):
      agc_item = agc_entry()
      if args.scfg.get_spcagc_item(sidx, aidx, agc_item) != True:
        print "Error: Get spcagc item fail!"
        continue
      
      print "start restructure data:", sidx, aidx, ag_dmlist, out_dmlist
      dtmp = stat_grp(dtmp, agc_item)
      pass

  # genrate dst data

  #


if __name__ == "__main__":
  print "stat...."
  args =  stat_arg()
  t=threading.Timer(2, stat_tm_action, [args])
  t.start()


