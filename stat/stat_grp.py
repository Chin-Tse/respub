#!/usr/bin/python

import pandas as pd
import numpy as np
from pandas import Series, DataFrame

import time
import threading

def tm_action(args=[]):
    print 'timeout:'
    print args
    global t        #Notice: use global variable!
    t=threading.Timer(2, tm_action, [pline])
    t.start()
    # do data process


file='/home/see/src/stat/demo4.log'
pline=3

data3=pd.read_csv(file, header=None, parse_dates=[0], index_col=0, skiprows=pline)
print data3


if __name__ == "__main__":
  t=threading.Timer(2, tm_action, [pline])
  t.start()


