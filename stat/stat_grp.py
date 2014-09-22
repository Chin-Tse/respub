#!/usr/bin/python

import pandas as pd
import numpy as np
import configobj
import time
import threading
import IPy

from IPy import IP, IPSet

from pandas import Series, DataFrame
from configobj import ConfigObj

global config
config = ConfigObj('./config.txt')

def tm_action(args=[]):
    print 'timeout:'
    print args
    global t        #Notice: use global variable!
    t=threading.Timer(2, tm_action, [pline])
    t.start()
    # do data process

file='/home/see/src/respub/stat/demo4.log'
pline=3

data3=pd.read_csv(file, header=None, parse_dates=[0], index_col=0, skiprows=pline)
#srcip = IP(data3[2]+'/'+'24').net
dtmp = data3
dtmp[2] = dtmp[2].map(lambda x : str(IP(x).make_net(24)))
print dtmp[2]



class auto_gen_cfg(object):
  def __init__(self, section, ksec):
    self.acfg = section
    self.alen = len(section)
    self.acfg_name = []
    self.acfg_kidx = []
    self.acfg_key = []
    for icfg, acfg_item in enumerate(self.acfg):
      #cfg-name
      self.acfg_name.append(acfg_item)
      self.acfg_kidx.append([])
      self.acfg_key.append([])
      #self.acfg[icfg][0] = acfg_item
      for ikey, key in enumerate(self.acfg[acfg_item]):
        try:
          print ikey, key
          self.acfg_key[icfg].append(key)
          self.acfg_kidx[icfg].append(int(ksec[key]))
          pass
        except KeyError:
          raise KeyError("%r, Invalid config key! %r", key, ksec.keys())

    print self.acfg_name
    print self.acfg_kidx
    print self.acfg_key
    
    pass


def parse_cfg():
  sec_opt = config['log-options']
  sec_agp = config['auto-grp']
  print sec_agp.values()[0][1]
  sec_spc = config['spc-grp']

  agc = auto_gen_cfg(sec_agp, sec_opt)


if __name__ == "__main__":
  parse_cfg()
  t=threading.Timer(2, tm_action, [pline])
  t.start()


