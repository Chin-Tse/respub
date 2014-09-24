#!/usr/bin/python
##
# @file stat_grp.py
# @brief Parse config file
# @author zhoujinze, zhoujz@chinanetcenter.com
# @version 0.0.1
# @date 2014-09-24

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


global config
cfgfile = './config.txt'
config = ConfigObj(cfgfile)

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


class auto_gcfg(object):
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
          raise ValueError("%r, Invalid config key! %r", key, ksec.keys())

    print self.acfg_name
    print self.acfg_kidx
    print self.acfg_key
    
    pass


def parse_agpstr(string):
  #convert to str
  agpstr = str(string)
  #strip space, all sutuation to one format
  agpstr = agpstr.replace(" ", "")
  agpstr = agpstr.replace("[", "")
  agpstr = agpstr.replace("]", "")
  agpstr = agpstr.replace("'", "")
  #print "RRR:", agpstr
  #find key domain
  index = agpstr.find('|')
  if index < 0:
    raise ValueError("Config do not have a right format!", str(string))
  acfg_dm = agpstr[:index]
  if (len(acfg_dm) == 0):
    acfg_dm="()"
  else:
    acfg_dm = acfg_dm.replace(acfg_dm[ - 1], acfg_dm[ - 1] + '\')', 1)
    acfg_dm = acfg_dm.replace(acfg_dm[0],  '(\'' + acfg_dm[0], 1)
    acfg_dm = acfg_dm.replace(",", '\', \'')
    #print '--------------', acfg_dm
  acfg_key = list(eval(acfg_dm))

  #find value domain
  agpstr = agpstr[index + 1:]
  index = agpstr.find('|')
  if index < 0:
    raise ValueError("Config do not have a right format!", str(string))
  acfg_value = agpstr[:index]
  acfg_value = acfg_value.replace(" ", "")
  #print "acfg_value", acfg_value

  #find threshold
  acfg_thd = agpstr[index+1:]
  if acfg_thd.isdigit() != True:
    raise ValueError("Config do not have a right format!", str(string))
  else:
    #print 'success'
    pass

  return acfg_key, acfg_value, acfg_thd


class spc_gcfg(object):
  def __init__(self, section, ksec):
    self.scfg = section
    self.slen = len(section)

    self.scfg_name = []
    self.scfg_keyidx = []
    self.scfg_keyname = []
    self.scfg_keymatch = []
    self.scfg_agp = []

    for iscfg , scfg_item in enumerate(self.scfg):
      self.scfg_name.append(scfg_item)
      self.scfg_keyidx.append([])
      self.scfg_keyname.append([])
      self.scfg_keymatch.append([])

      self.scfg_agp.append([])
      #first elem save group number
      self.scfg_agp[iscfg].append(0)

      print self.scfg[scfg_item]
      for iagp, agp_item in enumerate(self.scfg[scfg_item]):
        print iagp, agp_item, self.scfg[scfg_item][agp_item]
        if agp_item in ksec.keys():
          try:
            self.scfg_keyname[iscfg].append(agp_item)
            self.scfg_keyidx[iscfg].append(int(ksec[agp_item]))
            self.scfg_keymatch[iscfg].append(self.scfg[scfg_item][agp_item])
          except:
            pass
          print '---scfg---', iscfg, self.scfg_keyname[iscfg], \
            self.scfg_keyidx[iscfg], self.scfg_keymatch[iscfg]
        else:
          #auto group item
          self.scfg_agp[iscfg][0] += 1
          a_idx = self.scfg_agp[iscfg][0]
          
          keylist, value, threshold = parse_agpstr(self.scfg[scfg_item][agp_item])
          print '----what---', keylist

          self.scfg_agp[iscfg].append([])
          #save agp cfg: name, value_name, value_col, threshold
          self.scfg_agp[iscfg][a_idx].append(agp_item)
          if value in ksec.keys():
            self.scfg_agp[iscfg][a_idx].append(value)
            self.scfg_agp[iscfg][a_idx].append(ksec[value])
            self.scfg_agp[iscfg][a_idx].append(int(threshold))
          else:
            raise ValueError("Config do not have a right format!", str(self.scfg[scfg_item][agp_item]))

          #keyidx list and keyname list
          self.scfg_agp[iscfg][a_idx].append([])
          self.scfg_agp[iscfg][a_idx].append([])
          self.scfg_agp[iscfg][a_idx].append([])

          # i don't know how to use maroc now, so 4/5...
          for ikey, key in enumerate(list(keylist)):
            try:
              print '--', ikey, key
              self.scfg_agp[iscfg][a_idx][4].append(int(ksec[key]))
              self.scfg_agp[iscfg][a_idx][5].append(key)
            except:
              pass
            
          print '---', iscfg, a_idx, self.scfg_agp[iscfg][a_idx]

  # get spcfg number
  def get_spcfg_num(self):
    return (self.slen)

  # get sidx-th spcfg's special part
  def get_spc_name(self, sidx):
    if sidx >= self.slen:
      return None
    return self.scfg_name
    
  def get_spc_dmlist(self, sidx):
    if sidx >= self.slen:
      return []    
    return self.scfg_keyidx[sidx]
  def get_spc_dmmatch(self, sidx):
    if sidx >= self.slen:
      return []    
    return self.scfg_keymatch[sidx]

  def get_spc_dmname(self, sidx):
    if sidx >= self.slen:
      return []    
    return self.scfg_keyname[sidx]

  def get_spcagc_num(self, sidx):
    if sidx >= self.slen:
      return 0
    return self.scfg_agp[sidx][0]

  def get_spcagc_name(self, sidx, aidx):
    if sidx >= self.slen or aidxi + 1 >= self.get_spcagc_num(sidx):
      return None
    return self.scfg_agp[sidx][aidx + 1][0]
 
  def get_spcagc_dmlist(self, sidx, aidx):
    if sidx >= self.slen or aidxi + 1 >= self.get_spcagc_num(sidx):
      return []
    return self.scfg_agp[sidx][aidx + 1][4]
    
  def get_spcagc_dmname(self, sidx, aidx):
    if sidx >= self.slen or aidxi + 1 >= self.get_spcagc_num(sidx):
      return []
    return self.scfg_agp[sidx][aidx + 1][5]
  
  def get_spcagc_statcol(self, sidx, aidx):
    if sidx >= self.slen or aidxi + 1 >= self.get_spcagc_num(sidx):
      return []
    return self.scfg_agp[sidx][aidx + 1][2]

  def get_spcagc_statcol_name(self, sidx, aidx):
    if sidx >= self.slen or aidxi + 1 >= self.get_spcagc_num(sidx):
      return []
    return self.scfg_agp[sidx][aidx + 1][1]

  def get_spcagc_threshold(self, sidx, aidx):
    if sidx >= self.slen or aidxi + 1 >= self.get_spcagc_num(sidx):
      return []
    return self.scfg_agp[sidx][aidx + 1][3]


def getcfg(filename):
  return config

def parse_cfg(filename):
  sec_path = config['path']
  sec_opt = config['log-format']
  sec_agp = config['auto-grp']
  sec_sgp = config['spc-grp']
  return sec_path, sec_opt, sec_agp, sec_sgp

if __name__ == "__main__":
  sec_opt, sec_agp, sec_sgp = parse_cfg('./config.txt')
  spc = spc_gcfg(sec_sgp, sec_opt)
  t=threading.Timer(2, tm_action, [pline])
  t.start()


