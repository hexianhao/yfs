#ifndef lock_server_cache_h
#define lock_server_cache_h

#include <string>
#include <set>
#include <map>
#include <mutex>
#include <condition_variable>

#include "lock_protocol.h"
#include "rpc.h"
#include "lock_server.h"


class lock_server_cache {
 private:
  int nacquire;
 public:
  lock_server_cache();
  lock_protocol::status stat(lock_protocol::lockid_t, int &);
  int acquire(lock_protocol::lockid_t, std::string id, int &);
  int release(lock_protocol::lockid_t, std::string id, int &);

  int revoke(lock_protocol::lockid_t, int &);
  int retry(lock_protocol::lockid_t, int &);

 private:
  enum lock_state {
    FREE,
    LOCKED,
    // 为了防止出现deadlock，需要以下两个状态
    LOCKED_AND_WAIT,  // 表示有其它客户端等待锁
    RETRYING          // 表示服务器提醒客户端重试
  };

  struct lock_entry {
    // 当前锁的持有者
    std::string owner;
    // 等待集合
    std::set<std::string> waitSet;
    // 当前锁的状态
    lock_state state;

    lock_entry() : state(FREE) {}
  };

  std::map<lock_protocol::lockid_t, lock_entry> lockMap;

  std::mutex mtx;
};

#endif
