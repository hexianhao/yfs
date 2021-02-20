#ifndef lock_server_cache_rsm_h
#define lock_server_cache_rsm_h

#include <string>
#include <map>
#include <set>
#include <mutex>

#include "lock_protocol.h"
#include "rpc.h"
#include "rsm_state_transfer.h"
#include "rsm.h"
#include "fifo.h"

class lock_server_cache_rsm : public rsm_state_transfer {
 private:
  int nacquire;
  class rsm *rsm;
  
  enum lock_state {
    FREE,
    LOCKED,
    // 为了防止出现deadlock，需要以下两个状态
    LOCKED_AND_WAIT,  // 表示有其它客户端等待锁
    RETRYING          // 表示服务器提醒客户端重试
  };

  struct lock_entry {
      std::string owner;
      std::set<std::string> waitSet;
      lock_state state;
  };
  
  std::map<lock_protocol::lockid_t, lock_entry> lockMap;
  std::mutex mtx;

 public:
  lock_server_cache_rsm(class rsm *rsm = 0);
  lock_protocol::status stat(lock_protocol::lockid_t, int &);
  void revoker();
  void retryer();
  std::string marshal_state();
  void unmarshal_state(std::string state);
  int acquire(lock_protocol::lockid_t, std::string id, 
	      lock_protocol::xid_t, int &);
  int release(lock_protocol::lockid_t, std::string id, lock_protocol::xid_t,
	      int &);
};

#endif
