#!/usr/bin/python
##
# @file stat.py
# @brief stat
# @author zhoujinze, zhoujz@chinanetcenter.com
# @version 0.0.1
# @date 2014-09-28

import pandas as pd
import numpy as np
import configobj
import os
import time
import threading
import copy

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
# maybe should be moved to stat_cfg module
class stat_arg(object):
  def __init__(self, cfgfile='./config.txt'):
    self.pline = 0;
    self.cfgfile = cfgfile
    self.cfgtime = (os.stat(self.cfgfile).st_mtime)
    self.config = stat_cfg.getcfg(self.cfgfile);
    self.cfg_fmt = self.config['log-format']
    self.out_fmt = self.config['outlog-format']
    self.cfg_agp = self.config['auto-grp'] 
    self.cfg_sgp = self.config['spc-grp']
    self.input_log = self.config['path']['input']
    self.output_log = self.config['path']['output']
    opath = os.path.dirname(self.output_log)
    if os.path.exists(opath) == False:
      os.makedirs(opath)
    self.output_fd = open(self.output_log, 'w')
    self.scfg = spc_gcfg(self.cfg_sgp, self.cfg_fmt, self.out_fmt)
    self.acfg = auto_cfg(self.cfg_agp, self.cfg_fmt, self.out_fmt)
    self.max_col = 0
    self.max_col = max(int(col) for col in self.cfg_fmt.values()) 
    self.max_col += 1
    self.cfg_fmt['src_net'] = self.max_col
    self.max_col += 1
    self.cfg_fmt['dst_net'] = self.max_col

  def chkcfg(self):
    #check cfgfile change date
    if self.cfgtime == (os.stat(self.cfgfile).st_mtime):
      return
    
    self.cfgtime = (os.stat(self.cfgfile).st_mtime)
    # reload config file
    self.config.reload()
    self.cfg_fmt = self.config['log-format']
    self.out_fmt = self.config['outlog-format']
    self.cfg_agp = self.config['auto-grp'], 
    self.cfg_sgp = self.config['spc-grp']

    #pline
    if self.input_log != self.config['path']['input']:
      self.pline = 0
      self.input_log = self.config['path']['input']
    if self.output_log != self.config['path']['output']:
      self.output_log = self.config['path']['output']
      opath = os.path.dirname(self.output_log)
      if os.path.exists(opath) == False:
        os.makedirs(opath)
      self.output_fd.close()
      self.output_fd = open(self.output_log, 'w')
    print self.input_log
    #xxxx: maybe cause mem leak here, don't care now
    self.scfg = spc_gcfg(self.cfg_sgp, self.cfg_fmt, self.out_fmt)
    self.max_col = 0
    self.max_col = max(int(col) for col in self.cfg_fmt.values()) 
    self.max_col += 1
    self.cfg_fmt['src_net'] = self.max_col
    self.max_col += 1
    self.cfg_fmt['dst_net'] = self.max_col

  # check outlog size and inputlog size
  def chksize(self):
    pass

  def write(self, hdr, dstr):
    self.output_fd.writelines([hdr, "---------------\n", dstr])
 
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
    #print '--------', dmlist[idx], cfg_fmt['srcip'], cfg_fmt['dstip']
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
  #print cmpstr
  return data[eval(cmpstr)]

def stat_recur_stat(df, dmlist, acfg_item):
  dnr = 0
  dstr = ""
  dlen = len(dmlist)
  dmidx = len(acfg_item.dmlist) - dlen
  if dlen < 1:
    return dnr, dstr
  gp = df.groupby(dmlist[0])
  for gkey, sdf in gp:
    val_list = list(gp.sum().loc[gkey, acfg_item.stat_cols])
    dbgstr = str(gkey) + ":" + str(acfg_item.stat_name) + str(val_list)

    # check current dm val-threshold
    for val_idx, val in enumerate(val_list):
      if val >= acfg_item.threshold[val_idx]:
        #goto next level
        break;
    # goto next gkey directly
    else:
      #print 'pass:', gkey, val_list, acfg_item.threshold
      continue
    # button level
    if dlen == 1:
      dnr += 1
      dstr += "%*s:%*s"\
          %(acfg_item.pre_space[dmidx] + len(acfg_item.dmname[dmidx]),\
          acfg_item.dmname[dmidx], \
          acfg_item.dm_space[dmidx], gkey)
      
      # value output
      for vidx, val in enumerate(val_list):
        if vidx == 0:
          dstr += "%*d"%(acfg_item.dm2stat_space[dmidx] + acfg_item.stat_space[vidx], val)
        else:
          dstr += "%*d"%(acfg_item.stat_space[vidx], val)
      dstr += "\n"
      continue
    else:
      mstr = "%-*s:%-*s"%(acfg_item.pre_space[dmidx], acfg_item.dmname[dmidx], \
          acfg_item.dm_space[dmidx], str(gkey))
      for vidx, val in enumerate(val_list):
        if vidx == 0:
          mstr += "%*d"%(acfg_item.dm2stat_space[dmidx] + acfg_item.stat_space[vidx], val)
        else:
          mstr += "%*s"%(acfg_item.stat_space[vidx], val)
      mstr += "\n"
      # goto next level
      sublist = copy.deepcopy(dmlist[1:])
      nr, ostr = stat_recur_stat(sdf, sublist, acfg_item)
      if nr > 0:
        dnr += nr
        dstr += mstr + ostr

  return dnr, dstr
  
# df would be change after this process
def stat_grp(df, acfg_item):
  dm_len = len(acfg_item.dmlist)  
  if dm_len < 1:
    return

  dmlist = copy.deepcopy(acfg_item.dmlist)
  dnr, dstr = stat_recur_stat(df, dmlist, acfg_item)

  hdr = ""
  agc_hdr = ""
  if dnr > 0:
    agc_hdr += str(acfg_item.name) + ":\n"
    for idx, name in enumerate(acfg_item.dmname):
      hdr_len = len(hdr)
      sp = acfg_item.pre_space[idx] - hdr_len + acfg_item.dm_space[idx]
      hdr += "%*s"%(sp, name)

    for idx, name in enumerate(acfg_item.stat_name):
      if idx == 0:
        sp = acfg_item.dm_space[-1] + acfg_item.dm2stat_space[-1] \
            + acfg_item.stat_space[idx]
      else:
        sp = acfg_item.stat_space[idx]
      hdr += "%*s"%(sp, name)
    hdr = agc_hdr + hdr + "\n"
    # ouput to output logfile

  return dnr, hdr, dstr

def stat_tm_action(args=[]):
  print 'timeout:'
  #arg = args[0]
  global t        #Notice: use global variable!
  t=threading.Timer(5, stat_tm_action, [args])
  t.start()

  # read data
  print args.input_log
  try:
    data = pd.read_csv(args.input_log, header=None, \
        nrows=120, parse_dates=[0], skiprows=args.pline)

    data_len = len(data)
    args.pline += data_len
  except:
    print "Input Logfile End......"
    return
  
  slen = args.scfg.get_spcfg_num()
  #print "slen:", slen 
  agc_item = acfg_entry()
 
  alen = args.acfg.get_acfg_num()
  for aidx in range(alen):
    if args.acfg.get_acfg_item(aidx, agc_item) != True:
      print "Error: Get spcagc item fail!"
      continue

    print "start stat agp_data:", aidx, \
        agc_item.dmname, agc_item.stat_name
    dnr, hdr, dstr = stat_grp(data, agc_item)
    #wirte to file
    if dnr > 0:
      hdr = sghdr + hdr
      args.write(hdr, dstr)
      print hdr
      print dstr

  for sidx in range(slen):
    dtmp = stat_prepare(data, args, sidx)
    sghdr =""
    sghdr = args.scfg.get_spc_name(sidx) + ":"
    for aidx in range(args.scfg.get_spcagc_num(sidx)):
      if args.scfg.get_spcagc_item(sidx, aidx, agc_item) != True:
        print "Error: Get spcagc item fail!"
        continue
      
      print "start restructure data:", sidx, aidx, \
          agc_item.dmname, agc_item.stat_name
      dnr, hdr, dstr = stat_grp(dtmp, agc_item)
      #wirte to file
      if dnr > 0:
        hdr = sghdr + hdr
        args.write(hdr, dstr)
        print hdr
        print dstr
 


if __name__ == "__main__":
  print "stat...."
  #print  time.mktime(os.stat("./config.txt").st_mtime)
  print  (os.stat("./config.txt").st_mtime)
  args =  stat_arg()
  t=threading.Timer(2, stat_tm_action, [args])
  t.start()


