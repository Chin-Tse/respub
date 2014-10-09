#!/usr/bin/python
##
# @file stat.py
# @brief stat
# @author zhoujinze, zhoujz@chinanetcenter.com
# @version 0.0.1
# @date 2014-09-28

import configobj
import os, stat
import time
import threading
import copy

import sys
import types

import IPy
import stat_cfg

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

def get_input_linenr(args):
  wcline_cmd = "wc -l " + args.input_log
  wcline = os.popen(wcline_cmd).readlines()
  #print wcline, wcline[0].find(" "), wcline[0][0:wcline[0].find(" ")]
  linenr = int(wcline[0][0:wcline[0].find(" ")])
  print wcline, linenr
  return linenr

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
    self.in_log_ctime = (os.stat(self.cfgfile).st_ctime)
    self.output_log = self.config['path']['output']
    opath = os.path.dirname(self.output_log)
    if os.path.exists(opath) == False:
      os.makedirs(opath)
    self.output_fd = open(self.output_log, 'w')
    # count current line of input log
    if os.path.exists(self.input_log) == False:
      raise ValueError("Error cfg, input log is not exist!", self.input_log)

    wcline_cmd = "wc -l " + self.input_log
    wcline = os.popen(wcline_cmd).readlines()
    #print wcline, wcline[0].find(" "), wcline[0][0:wcline[0].find(" ")]
    self.pline = int(wcline[0][0:wcline[0].find(" ")])
    print "init pline:", self.pline

    self.scfg = spc_gcfg(self.cfg_sgp, self.cfg_fmt, self.out_fmt)
    self.acfg = auto_cfg(self.cfg_agp, self.cfg_fmt, self.out_fmt)
    self.max_col = 0
    self.max_col = max(int(col) for col in self.cfg_fmt.values()) 
    self.cfg_fmt['src_net'] = self.max_col + 1
    self.cfg_fmt['dst_net'] = self.max_col + 2

  def chkcfg(self):
    #check cfgfile change date
    cfg_mtime = os.stat(self.cfgfile).st_mtime
    if self.cfgtime == cfg_mtime:
      return
    
    self.cfgtime = cfg_mtime
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
    self.cfg_fmt['src_net'] = self.max_col + 1
    self.cfg_fmt['dst_net'] = self.max_col + 2

  # check outlog size and inputlog size
  def chksize(self):
    in_log_st = os.stat(self.input_log)
    if self.in_log_ctime != in_log_st.st_ctime:
      self.in_log_ctime = in_log_st.st_ctime
      print "logfile change:", self.in_log_ctime
      self.pline = 0

    """
    if in_log_st[stat.ST_SIZE] > 2000000:
      pass
    """
    pass

  def write(self, hdr, dstr):
    self.output_fd.writelines([hdr, "---------------\n", dstr])
 
# df would be change after this process
def stat_tm_action(args=[]):
  print 'timeout:'
  #arg = args[0]
  # chk
  args.chkcfg()
  print args.input_log, args.pline
  slen = args.scfg.get_spcfg_num()

  agc_item = acfg_entry()
  alen = args.acfg.get_acfg_num()
  for aidx in range(alen):
      pass;
  for sidx in range(slen):
      pass;

  del agc_item

  global t        #Notice: use global variable!
  t=threading.Timer(5, stat_tm_action, [args])
  t.start()

if __name__ == "__main__":
  print "stat...."
  #print  time.mktime(os.stat("./config.txt").st_mtime)
  print  (os.stat("./config.txt").st_mtime)
  args =  stat_arg()
  t=threading.Timer(2, stat_tm_action, [args])
  t.start()


