// yfs client.  implements FS operations using extent and lock server
#include "yfs_client.h"
#include "extent_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


yfs_client::yfs_client(std::string extent_dst, std::string lock_dst)
{
  ec = new extent_client(extent_dst);

}

yfs_client::inum
yfs_client::n2i(std::string n)
{
  std::istringstream ist(n);
  unsigned long long finum;
  ist >> finum;
  return finum;
}

std::string
yfs_client::filename(inum inum)
{
  std::ostringstream ost;
  ost << inum;
  return ost.str();
}

bool
yfs_client::isfile(inum inum)
{
  if(inum & 0x80000000)
    return true;
  return false;
}

bool
yfs_client::isdir(inum inum)
{
  return ! isfile(inum);
}

int
yfs_client::getfile(inum inum, fileinfo &fin)
{
  int r = OK;

  printf("getfile %016llx\n", inum);
  extent_protocol::attr a;
  if (ec->getattr(inum, a) != extent_protocol::OK) {
    r = IOERR;
    goto release;
  }

  fin.atime = a.atime;
  fin.mtime = a.mtime;
  fin.ctime = a.ctime;
  fin.size = a.size;
  printf("getfile %016llx -> sz %llu\n", inum, fin.size);

 release:

  return r;
}

int
yfs_client::getdir(inum inum, dirinfo &din)
{
  int r = OK;

  printf("getdir %016llx\n", inum);
  extent_protocol::attr a;
  if (ec->getattr(inum, a) != extent_protocol::OK) {
    r = IOERR;
    goto release;
  }
  din.atime = a.atime;
  din.mtime = a.mtime;
  din.ctime = a.ctime;

 release:
  return r;
}

yfs_client::inum yfs_client::random_inum(bool isfile) {
  inum ret = (unsigned long long)((rand() & 0x7fffffff) | (isfile << 31));
  ret = 0xffffffff & ret;

  return ret;
}

int yfs_client::create(inum parent, const char* name, inum& inum) {
  std::string data;
  std::string filename;
  // 先判断父目录
  if (ec->get(parent, data) != extent_protocol::OK) {
    return IOERR;
  }

  // 判断文件是否存在
  filename = "/" + std::string(name) + "/";
  if (data.find(filename) != std::string::npos) {
    return EXIST;
  }

  inum = random_inum(true);
  if (ec->put(inum, std::string()) != extent_protocol::OK) {
    return IOERR;
  }

  data.append(filename + std::to_string(inum) + "/");
  if (ec->put(parent, data) != extent_protocol::OK) {
    return IOERR;
  }

  return OK;
}

int yfs_client::read(inum inum, int off, int size, std::string& buf) {
  std::string data;
  if (ec->get(inum, data) != extent_protocol::OK) {
    return IOERR;
  }

  if (off >= data.size()) {
    buf = std::string();
    return OK;
  }

  if (off + size >= data.size()) {
    size = data.size() - off;
  }
  buf = data.substr(off, size);

  return OK;
}

int yfs_client::write(inum inum, int off, int size, const char* buf) {
  std::string data;
  if (ec->get(inum, data) != extent_protocol::OK) {
    return IOERR;
  }

  if (off + size >= data.size()) {
    data.resize(off + size, '\0');
  }

  for(int i = 0; i < size; i++) {
    data[off + i] = buf[i];
  }

  if (ec->put(inum, data) != extent_protocol::OK) {
    return IOERR;
  }

  return OK;
}

int yfs_client::lookup(inum parent, const char* name, inum& inum, bool* found) {
  std::string data;
  if (ec->get(parent, data) != extent_protocol::OK) {
    return IOERR;
  }

  std::string filename = "/" + std::string(name) + "/";
  size_t pos = data.find(filename);
  if (pos != std::string::npos) {
    *found = true;
    pos += filename.length();
    size_t end = data.find_first_of('/', pos);
    if (end != std::string::npos) {
      std::string ino = data.substr(pos, end - pos);
      inum = n2i(ino);
      return OK;
    } else {
      return IOERR;
    }
  } else {
    return IOERR;
  }
}

int yfs_client::setattr(inum inum, struct stat* attr) {
  size_t size = attr->st_size;
  std::string buf;
  
  if (ec->get(inum, buf) != extent_protocol::OK) {
    return IOERR;
  }

  // 用resize修改buf的大小
  buf.resize(size, '\0');
  if (ec->put(inum, buf) != extent_protocol::OK) {
    return IOERR;
  }

  return OK;
}

int yfs_client::readdir(inum parent, std::list<dirent>& dirents) {
  std::string data;
  if (ec->get(parent, data) != extent_protocol::OK) {
    return IOERR;
  }

  size_t pos = 0;
  while (pos < data.length()) {
    dirent d;
    pos = data.find("/", pos);
    if (pos == std::string::npos) {
      break;
    }

    size_t end = data.find_first_of('/', pos + 1);
    size_t len = end - pos - 1;
    d.name = data.substr(pos + 1, len);

    pos = end;
    end = data.find_first_of('/', pos + 1);
    len = end - pos - 1;
    std::string inum_str = data.substr(pos + 1, len);
    d.inum = n2i(inum_str);

    dirents.push_back(d);
    pos = end + 1;
  }
}

