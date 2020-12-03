// lock protocol

#ifndef lock_protocol_h
#define lock_protocol_h

#include <mutex>
#include <condition_variable>

#include "rpc.h"

class lock_protocol {
 public:
  enum xxstatus { OK, RETRY, RPCERR, NOENT, IOERR };
  typedef int status;
  typedef unsigned long long lockid_t;
  enum rpc_numbers {
    acquire = 0x7001,
    release,
    stat
  };
};

class lock {
public:
  enum lock_status { FREE, LOCKED };
  // 每个锁的id
  lock_protocol::lockid_t m_lid;
  // status
  int m_state;
  // condition variable
  std::condition_variable cv;

  // 构造函数
  lock() = default;
  lock(lock_protocol::lockid_t lid, int state) : m_lid(lid), m_state(state) { }
};

#endif 
