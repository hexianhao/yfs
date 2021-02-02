#ifndef yfs_client_h
#define yfs_client_h

#include <string>
#include "lock_protocol.h"
#include "extent_client.h"
#include "lock_client.h"
#include <vector>

class LockGuard {
public:
  LockGuard() = default;
  
  LockGuard(lock_client *lc, lock_protocol::lockid_t lid) : m_lc(lc), m_lid(lid) {
    m_lc->acquire(m_lid);
  }

  ~LockGuard() {
    m_lc->release(m_lid);
  }

private:
  lock_client *m_lc;
  lock_protocol::lockid_t m_lid;
};

class yfs_client {
  extent_client *ec;
  lock_client *lc;    // lock server
 public:

  typedef unsigned long long inum;
  enum xxstatus { OK, RPCERR, NOENT, IOERR, EXIST };
  typedef int status;

  struct fileinfo {
    unsigned long long size;
    unsigned long atime;
    unsigned long mtime;
    unsigned long ctime;
  };
  struct dirinfo {
    unsigned long atime;
    unsigned long mtime;
    unsigned long ctime;
  };
  struct dirent {
    std::string name;
    yfs_client::inum inum;
  };

 private:
  static std::string filename(inum);
  static inum n2i(std::string);
  inum random_inum(bool isfile);

 public:

  yfs_client(std::string, std::string);

  bool isfile(inum);
  bool isdir(inum);

  int getfile(inum, fileinfo &);
  int getdir(inum, dirinfo &);

  int create(inum, const char*, inum &);
  int read(inum, int, int, std::string &);
  int write(inum, int, int, const char*);
  int lookup(inum, const char*, inum &, bool*);

  int mkdir(inum, const char*, inum &);
  int unlink(inum, const char*);

  int setattr(inum, struct stat*);
  int readdir(inum, std::list<dirent>&);
};

#endif 
