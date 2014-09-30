#!/usr/bin/python
##
# @file stat_grp.py
# @brief Parse config file
# @author zhoujinze, zhoujz@chinanetcenter.com
# @version 0.0.1
# @date 2014-09-24

import configobj
import time
import IPy
import copy

import sys
import types

from IPy import IP, IPSet
from configobj import ConfigObj

if sys.version_info >= (3,):
    INT_TYPES = (int,)
    STR_TYPES = (str,)
    xrange = range
else:
    INT_TYPES = (int, long)
    STR_TYPES = (str, unicode)


global config

# convert like "abc, cde, dcc, 10" -> ['abc', 'cde', 'ddc', 10]
def string2list(lstr):
  if (len(lstr) == 0):
    lstr = "[]"
  else:
    lstr = lstr[:-1] + lstr[-1] + '\']'
    #lstr = lstr.replace(lstr[-1], lstr[-1] + '\']', 1)
    lstr = lstr.replace(lstr[0],  '[\'' + lstr[0], 1)
    lstr = lstr.replace(",", '\', \'')
    #print '--------------', lstr
  lst = list(eval(lstr))
  return lst

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
  acfg_key = string2list(acfg_dm)

  #find value domain
  agpstr = agpstr[index + 1:]
  index = agpstr.find('|')
  if index < 0:
    raise ValueError("Config do not have a right format!", str(string))
  acfg_colname = agpstr[:index]
  acfg_colname = acfg_colname.replace(" ", "")
  acfg_stat_colname = string2list(acfg_colname)

  #find threshold
  acfg_thdstr = agpstr[index+1:]
  acfg_thds = string2list(acfg_thdstr)
  for idx, thd in enumerate(acfg_thds):
    if thd.isdigit() != True:
      raise ValueError("Config do not have a right format!", str(string), thd)
    acfg_thds[idx] = int(thd)

  return acfg_key, acfg_stat_colname, acfg_thds

##### Maybe there should had a class define every acfg_item ####
class acfg_entry():
  def __init__(self):
    self.name = ""
    self.dmlist = []
    self.dmname = []
    self.dm_space = []
    self.pre_space = []
    self.dm2stat_space = []
    self.stat_cols = []
    self.stat_name = []
    self.stat_space = []
    self.threshold = []
  
  #format output, calculate the space number
  def set_space(self):
    for idx, name in enumerate(self.dmname):
      if idx == 0:
        sp = 0
      else:
        sp = 1 + self.pre_space[idx - 1] \
            + len(self.dmname[idx - 1]) + self.dm_space[idx -1]

      self.pre_space.append(sp)

    stat_start = self.pre_space[-1] + len(self.dmname[-1]) \
        + self.dm_space[-1] + 8
    
    for idx, name in enumerate(self.dmname):
      sp = stat_start \
          - self.pre_space[idx] - len(self.dmname[idx]) \
          - self.dm_space[idx]

      self.dm2stat_space.append(sp)

class auto_cfg(object):
  def __init__(self, section, icfg, ocfg):
    self.acfg = section
    self.alen = len(section)
    self.acfg_name = []
    self.acfg_stat_name= []
    self.acfg_stat_cols= []
    self.acfg_threshold = []
    self.acfg_stat_space = []
    self.acfg_kidx = []
    self.acfg_key = []
    self.dm_space = []

    for aidx, acfg_item in enumerate(self.acfg):
      #cfg-name
      print self.acfg[acfg_item]
      keylist, value, threshold = parse_agpstr(self.acfg[acfg_item])
      self.acfg_name.append(acfg_item)
      self.acfg_stat_name.append([])
      self.acfg_stat_cols.append([])
      self.acfg_stat_space.append([])
      self.acfg_threshold.append([])

      for v, t in zip(value, threshold):
        if v in icfg.keys():
          self.acfg_stat_name[aidx].append(v)
          self.acfg_stat_cols[aidx].append(int(icfg[v]))
          self.acfg_threshold[aidx].append(int(t))
        else:
          raise ValueError("Config do not have a right format!", str(self.acfg[acfg_item]))

        if v in ocfg.keys():
          self.acfg_stat_space[aidx].append(int(ocfg[v]))
        else:
          self.acfg_stat_space[aidx].append(10)

      self.acfg_kidx.append([])
      self.acfg_key.append([])
      self.dm_space.append([])

      for ikey, key in enumerate(keylist):
        try:
          #print ikey, key
          self.acfg_key[aidx].append(key)
          self.acfg_kidx[aidx].append(int(icfg[key]))
          pass
        except KeyError:
          raise ValueError("%r, Invalid config key! %r", key, icfg.keys())

        if key in ocfg.keys():
          self.dm_space[aidx].append(int(ocfg[key]))
        else:
          self.dm_space[aidx].append(10)

      print "acfg:", aidx, self.acfg_name, self.acfg_key[aidx], self.acfg_kidx[aidx], \
          self.acfg_stat_name[aidx], self.acfg_stat_cols[aidx], self.acfg_threshold[aidx]

  def get_acfg_num(self):
    return self.alen

  def get_acfg_name(self, aidx):
    if aidx >= self.alen:
      return None
    return self.acfg_name[aidx]

  def get_acfg_dmlist(self, aidx):
    if aidx >= self.alen:
      return []
    return self.acfg_kidx[aidx]
  
  def get_acfg_dmname(self, aidx):
    if aidx >= self.alen:
      return []
    return self.acfg_key[aidx]
  
  def get_acfg_dm_space(self, aidx):
    if aidx >= self.alen:
      return []
    return self.dm_space[aidx]
 
  def get_acfg_stat_cols(self, aidx):
    if aidx >= self.alen:
      return []
    return self.acfg_stat_cols[aidx]
  
  def get_acfg_stat_name(self, aidx):
    if aidx >= self.alen:
      return []
    return self.acfg_stat_name[aidx]
  
  def get_acfg_threshold(self, aidx):
    if aidx >= self.alen:
      return []
    return self.acfg_threshold[aidx]

  def get_acfg_stat_space(self, aidx):
    if aidx >= self.alen:
      return []
    return self.acfg_stat_space[aidx]


  def get_acfg_item(self, aidx, acfg_item):
    if aidx >= self.alen:
      return False

    acfg_item.name = self.get_acfg_name(aidx)
    acfg_item.dmlist = self.get_acfg_dmlist(aidx)
    acfg_item.dmname = self.get_acfg_dmname(aidx)
    acfg_item.dm_space = self.get_acfg_dm_space(aidx)
    acfg_item.stat_cols = self.get_acfg_stat_cols(aidx)
    acfg_item.stat_name = self.get_acfg_stat_name(aidx)
    acfg_item.stat_space = self.get_acfg_stat_space(aidx)
    acfg_item.threshold = self.get_acfg_threshold(aidx)

    acfg_item.set_space()
   
    return True
    
class spc_gcfg(object):
  def __init__(self, section, icfg, ocfg):
    self.scfg = section
    self.slen = len(section)

    self.scfg_name = []
    self.scfg_keyidx = []
    self.scfg_keyname = []
    self.scfg_keymatch = []
    self.scfg_matchidx = []
    self.scfg_agp = []

    for iscfg , scfg_item in enumerate(self.scfg):
      self.scfg_name.append(scfg_item)
      self.scfg_keyidx.append([])
      self.scfg_keyname.append([])
      self.scfg_keymatch.append([])
      self.scfg_matchidx.append([])

      self.scfg_agp.append([])
      #first elem save group number
      self.scfg_agp[iscfg].append(0)

      print self.scfg[scfg_item]
      for iagp, agp_item in enumerate(self.scfg[scfg_item]):
        #print iagp, agp_item, self.scfg[scfg_item][agp_item]
        if agp_item in icfg.keys():
          try:
            self.scfg_keyname[iscfg].append(agp_item)
            self.scfg_keyidx[iscfg].append(int(icfg[agp_item]))
            self.scfg_keymatch[iscfg].append(self.scfg[scfg_item][agp_item])
          except:
            pass
        else:
          #auto group item
          self.scfg_agp[iscfg][0] += 1
          a_idx = self.scfg_agp[iscfg][0]
          
          keylist, colname_list, thrd_list = parse_agpstr(self.scfg[scfg_item][agp_item])
          if len(colname_list) != len(thrd_list):
            raise ValueError("Config do not have a right format!", \
                str(self.scfg[scfg_item][agp_item]))

          self.scfg_agp[iscfg].append([])
          #save agp cfg: name, value_name, value_col, threshold
          self.scfg_agp[iscfg][a_idx].append(agp_item)
          #stat col name
          self.scfg_agp[iscfg][a_idx].append([])
          #stat col 
          self.scfg_agp[iscfg][a_idx].append([])
          #stat threshold
          self.scfg_agp[iscfg][a_idx].append([])
          #stat space
          self.scfg_agp[iscfg][a_idx].append([])
          
          for v, t in zip(colname_list, thrd_list):
            if v in icfg.keys():
              self.scfg_agp[iscfg][a_idx][1].append(v)
              self.scfg_agp[iscfg][a_idx][2].append(int(icfg[v]))
              self.scfg_agp[iscfg][a_idx][3].append(int(t))
            else:
              raise ValueError("Config do not have a right format!", \
                  str(self.scfg[scfg_item][agp_item]))

            if v in ocfg.keys():
              self.scfg_agp[iscfg][a_idx][4].append(int(ocfg[v]))
            else:
              self.scfg_agp[iscfg][a_idx][4].append(10)

          #keyidx list and keyname list
          self.scfg_agp[iscfg][a_idx].append([])
          self.scfg_agp[iscfg][a_idx].append([])
          self.scfg_agp[iscfg][a_idx].append([])

          # i don't know how to use maroc now, so 4/5...
          for ikey, key in enumerate(list(keylist)):
            try:
              self.scfg_agp[iscfg][a_idx][5].append(int(icfg[key]))
              self.scfg_agp[iscfg][a_idx][6].append(key)
            except:
              pass

            if key in ocfg.keys():
              self.scfg_agp[iscfg][a_idx][7].append(int(ocfg[key]))
            else:
              self.scfg_agp[iscfg][a_idx][7].append(10)
            
          print 'scfgapc:', iscfg, a_idx, self.scfg_agp[iscfg][a_idx]          
        
      print 'scfg:', iscfg, self.scfg_keyname[iscfg], \
            self.scfg_keyidx[iscfg], self.scfg_keymatch[iscfg]

  def chk_spcagc_cfg(self, sidx):
    # check agpconfig item, agpkeyname should not contain any word in spcfg
    spc_dmlist = self.get_spc_dmlist(sidx)
    if len(spc_dmlist) < 1:
      return
    for a_idx in range(self.get_spcagc_num(sidx)):
      spcagc_dmlist = self.get_spcagc_dmlist(iscfg, a_idx - 1)
      spcagc_dmname = self.get_spcagc_dmname(iscfg, a_idx - 1)
      if len(spcagc_dmname) != len(spcagc_dmlist):
        raise ValueError("Config Error:", spcagc_dmname, spcagc_dmlist)
      for idx, akeyidx in enumerate(spcagc_dmlist):
        if akeyidx in self.get_spc_dmlist(sidx):
          del spcagc_dmlist[idx]
          del spcagc_dmname[idx]
          #delete and warning
          print "Agpcfg domain is dupplicated spcfg:", spcagc_dmname, self.get_spc_dmname(sidx)
      pass
    pass

  # get spcfg number
  def get_spcfg_num(self):
    return (self.slen)

  # get sidx-th spcfg's special part
  def get_spc_name(self, sidx):
    if sidx >= self.slen:
      return None
    return self.scfg_name[sidx]
    
  def get_spc_dmlist(self, sidx):
    if sidx >= self.slen:
      return []    
    return self.scfg_keyidx[sidx]

  def get_spc_matchlist(self, sidx):
    if sidx >= self.slen:
      return []    
    return self.scfg_matchidx[sidx]

  def set_spc_matchlist(self, sidx, idxlist):
    if sidx >= self.slen:
      return []
    if len(idxlist) != len(self.scfg_keyidx[sidx]):
      print "Config convert Error:", self.scfg_keyname[sidx], self.scfg_keyidx[sidx], idxlist
    self.scfg_matchidx[sidx] = copy.deepcopy(idxlist)
    return

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
    if sidx >= self.slen or aidx + 1 > self.get_spcagc_num(sidx):
      return None
    return self.scfg_agp[sidx][aidx + 1][0]
 
  def get_spcagc_dmlist(self, sidx, aidx):
    if sidx >= self.slen or aidx + 1 > self.get_spcagc_num(sidx):
      return []
    return self.scfg_agp[sidx][aidx + 1][5]
    
  def get_spcagc_dmname(self, sidx, aidx):
    if sidx >= self.slen or aidx + 1 > self.get_spcagc_num(sidx):
      return []
    return self.scfg_agp[sidx][aidx + 1][6]
  
  def get_spcagc_stat_cols(self, sidx, aidx):
    if sidx >= self.slen or aidx + 1 > self.get_spcagc_num(sidx):
      return []
    return self.scfg_agp[sidx][aidx + 1][2]

  def get_spcagc_stat_name(self, sidx, aidx):
    if sidx >= self.slen or aidx + 1 > self.get_spcagc_num(sidx):
      return []
    return self.scfg_agp[sidx][aidx + 1][1]

  def get_spcagc_threshold(self, sidx, aidx):
    if sidx >= self.slen or aidx + 1 > self.get_spcagc_num(sidx):
      return []
    return self.scfg_agp[sidx][aidx + 1][3]
 
  def get_spcagc_stat_space(self, sidx, aidx):
    if sidx >= self.slen or aidx + 1 > self.get_spcagc_num(sidx):
      return []
    return self.scfg_agp[sidx][aidx + 1][4]
  
  def get_spcagc_dm_space(self, sidx, aidx):
    if sidx >= self.slen or aidx + 1 > self.get_spcagc_num(sidx):
      return []
    return self.scfg_agp[sidx][aidx + 1][7]
 
  def get_spcagc_item(self, sidx, aidx, acfg_item):
    if sidx >= self.slen or aidx + 1 > self.get_spcagc_num(sidx):
      return False
    acfg_item.name = self.get_spcagc_name(sidx, aidx)
    acfg_item.dmlist = self.get_spcagc_dmlist(sidx, aidx)
    acfg_item.dmname = self.get_spcagc_dmname(sidx, aidx)
    acfg_item.dm_space= self.get_spcagc_dm_space(sidx, aidx)
    acfg_item.stat_cols = self.get_spcagc_stat_cols(sidx, aidx) 
    acfg_item.stat_name = self.get_spcagc_stat_name(sidx, aidx) 
    acfg_item.stat_space = self.get_spcagc_stat_space(sidx, aidx) 
    acfg_item.threshold = self.get_spcagc_threshold(sidx, aidx) 

    acfg_item.set_space()
    return True

def getcfg(filename):
  global config
  config = ConfigObj(filename)
  return config

def parse_cfg(filename):
  global config
  config = ConfigObj(filename)
  sec_path = config['path']
  sec_opt = config['log-format']
  sec_agp = config['auto-grp']
  sec_sgp = config['spc-grp']
  out_fmt = config['outlog-format']
  return sec_path, sec_opt, sec_agp, sec_sgp, out_fmt

cfgfile = './config.txt'
if __name__ == "__main__":
  sec_path, sec_opt, sec_agp, sec_sgp, out_fmt = parse_cfg(cfgfile)
  spc = spc_gcfg(sec_sgp, sec_opt, out_fmt)
  print '================='
  agc = auto_cfg(sec_agp, sec_opt, out_fmt)

