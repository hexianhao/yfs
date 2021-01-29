// the extent server implementation

#include "extent_server.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

extent_server::extent_server() {}


int extent_server::put(extent_protocol::extentid_t id, std::string buf, int &)
{
  // You fill this in for Lab 2.
  std::lock_guard<std::mutex> lock(mtx);

  extent_protocol::attr attr;
  attr.atime = attr.mtime = attr.ctime = time(NULL);

  if (dataMap.find(id) != dataMap.end()) {
    attr.atime = dataMap[id].attr.atime;
  }
  attr.size = buf.size();
  dataMap[id].data = buf;
  dataMap[id].attr = attr;
  
  return extent_protocol::OK;
}

int extent_server::get(extent_protocol::extentid_t id, std::string &buf)
{
  // You fill this in for Lab 2.
  std::lock_guard<std::mutex> lock(mtx);

  if (dataMap.find(id) != dataMap.end()) {
    dataMap[id].attr.atime = time(NULL);
    buf = dataMap[id].data;
    return extent_protocol::OK;
  }

  return extent_protocol::NOENT;
}

int extent_server::getattr(extent_protocol::extentid_t id, extent_protocol::attr &a)
{
  // You fill this in for Lab 2.
  // You replace this with a real implementation. We send a phony response
  // for now because it's difficult to get FUSE to do anything (including
  // unmount) if getattr fails.
  std::lock_guard<std::mutex> lock(mtx);

  if (dataMap.find(id) != dataMap.end()) {
    a = dataMap[id].attr;
    return extent_protocol::OK;
  }

  return extent_protocol::NOENT;
}

int extent_server::remove(extent_protocol::extentid_t id, int &)
{
  // You fill this in for Lab 2.
  std::lock_guard<std::mutex> lock(mtx);

  if (dataMap.find(id) != dataMap.end()) {
    dataMap.erase(id);
    return extent_protocol::OK;
  }

  return extent_protocol::NOENT;
}
