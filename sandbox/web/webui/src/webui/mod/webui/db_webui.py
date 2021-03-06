#! /usr/bin/env python

"""
usage: %(progname)s [args]

"""

import os, sys, string, time, getopt

from pyclearsilver.log import *

from webui import config

from pyclearsilver import odb, hdfhelp, odb_sqlite3
from pyclearsilver import CSPage

from pyclearsilver.odb import *

from pyclearsilver import odb
from pyclearsilver import hdfhelp

from launchman import app

import urllib

def grabTopics(hdf, topics):
  topics = topics + ["/battery_state", "/power_board_state", "/app_status"]
  topicList = []
  for topic in topics: topicList.append("topic=%s" % topic)
  topics = string.join(topicList, "&")

  url = "http://localhost:8080/ros/receive?since=0&" + topics
  fp = urllib.urlopen(url)
  body = fp.read()
  fp.close()

  hdf.setValue("CGI.cur.messages", body)

def path2taskid(path):
  p = []
  package = None

  while path != "/":
    base, fn = os.path.split(path)

    p.insert(0, fn)
    if os.path.exists(os.path.join(base, "manifest.xml")):
      base, package = os.path.split(base)
      break
    path = base
    if path == "/":
      return None

  fn = apply(os.path.join, p)

  return os.path.join(package, fn)

class WebUIDB(odb.Database):
  def __init__(self,conn,debug=0):
    odb.Database.__init__(self, conn, debug=debug)

    self.addTable("robots", "ir_robots",  RobotTable, rowClass=Robot)
    self.addTable("apps", "ir_apps",  ApplicationTable, rowClass=Application)

  def defaultRowClass(self):
    return hdfhelp.HdfRow
  def defaultRowListClass(self):
    return hdfhelp.HdfItemList


      
    
class RobotTable(odb.Table):
  def _defineRows(self):
    self.d_addColumn("robotid",  kInteger,primarykey=1, unique=1, 
                     autoincrement=1)
    self.d_addColumn("name", kVarString)

class Robot(hdfhelp.HdfRow):
  pass

class ApplicationTable(odb.Table):
  def _defineRows(self):
    self.d_addColumn("appid",  kInteger,primarykey=1, unique=1, 
                     autoincrement=1)
    self.d_addColumn("taskid", kVarString, unique=1)
    self.d_addColumn("ordering", kVarString)
    self.d_addColumn("category", kVarString)

  def installApp(self, appfn):
    appfn = os.path.abspath(appfn)

    taskid = path2taskid(appfn)

    row = self.newRow()
    row.taskid = taskid
    row.save()

  def removeApp(self, taskid):
    row = self.lookup(taskid=taskid)
    row.delete()
    
  def listApps(self):
    rows = self.fetchAllRows()
    return rows

class Application(hdfhelp.HdfRow):
  def fetchApp(self, prefix, hdf):
    _app = app.App(str(self.taskid))
    doc = _app.load_yaml()

    hdf.setValue(prefix + "." + "package", _app.package)

    for key, val in doc.items():
      if val is not None:
        hdf.setValue(prefix + "." + key, val)
      else:
        hdf.setValue(prefix + "." + key, '')
  
    
def fullDBPath(path_to_store):
  return os.path.join(path_to_store, "webui.db3")

def initSchema(create=0, timeout=None):
  if timeout is None: timeout = 600

  path = config.getDBPath("webui")

  if create == 1:
    config.createDBPath(path)

  conn = odb_sqlite3.Connection(fullDBPath(path),
                                timeout=timeout)

  db = WebUIDB(conn,debug=debug)

  if create:
    db.createTables()
    db.synchronizeSchema()
    db.createIndices()

    if 0:
      if config.gWebUserID is not None and config.gWebGroupID is not None:
        config.webChown(fullDBPath(path))


  return db

def exists():
  path = config.getDBPath("webui")
  fn = fullDBPath(path)
  if os.path.exists(fn): 
    return 1
  return 0
  
  
def test():
  pass

def usage(progname):
  print __doc__ % vars()

def main(argv, stdout, environ):
  progname = argv[0]
  optlist, args = getopt.getopt(argv[1:], "", ["help", "test", "debug"])

  testflag = 0

  for (field, val) in optlist:
    if field == "--help":
      usage(progname)
      return
    elif field == "--debug":
      debugfull()
    elif field == "--test":
      testflag = 1

  if testflag:
    test()
    return

  db = initSchema(create=1)



if __name__ == "__main__":
  main(sys.argv, sys.stdout, os.environ)

  
  
