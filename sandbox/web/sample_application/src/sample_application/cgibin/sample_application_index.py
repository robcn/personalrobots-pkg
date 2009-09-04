#! /usr/bin/env python

"""
usage: %(progname)s [args]
"""

import os, sys, string, time, getopt, re
import neo_cgi, neo_util, neo_cs
from pyclearsilver.log import *
from pyclearsilver import CSPage, odb

from webui import MBPage

import db_webui

class MyPage(MBPage.MBPage):
  def setup(self, hdf):
    pass
    
  def display(self, hdf):
    db_webui.grabTopics(hdf, ['/chatter:more'])
    

def run(context):
  return MyPage(context, pagename="index", nologin=1)

def main():
  context = CSPage.Context()
  run(context).start()

if __name__ == "__main__":
  main()