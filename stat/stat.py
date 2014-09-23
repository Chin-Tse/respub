import pandas as pd
import numpy as np
import configobj
import time
import threading
import IPy

import sys
import types

from IPy import IP, IPSet

from pandas import Series, DataFrame
from configobj import ConfigObj

if sys.version_info >= (3,):
    INT_TYPES = (int,)
    STR_TYPES = (str,)
    xrange = range
else:
    INT_TYPES = (int, long)
    STR_TYPES = (str, unicode)

import stat_cfg
from stat_cfg import auto_gcfg, spc_gcfg

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

def stat_tm_action(args=[]):
  print 'timeout:'
  #arg = args[0]
  global t        #Notice: use global variable!
  t=threading.Timer(5, stat_tm_action, [args])
  t.start()

  # read data
  data = pd.read_csv(args.logfile, header=None, parse_dates=[0], index_col=0, skiprows=args.pline)
  print len(data)

  # genrate dst data

  #


if __name__ == "__main__":
  print "stat...."
  args =  stat_arg()
  t=threading.Timer(2, stat_tm_action, [args])
  t.start()


