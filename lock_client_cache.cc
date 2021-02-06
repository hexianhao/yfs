// RPC stubs for clients to talk to lock_server, and cache the locks
// see lock_client.cache.h for protocol details.

#include "lock_client_cache.h"
#include "rpc.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include "tprintf.h"


int lock_client_cache::last_port = 0;

lock_client_cache::lock_client_cache(std::string xdst, 
				     class lock_release_user *_lu)
  : lock_client(xdst), lu(_lu)
{
  srand(time(NULL)^last_port);
  rlock_port = ((rand()%32000) | (0x1 << 10));
  const char *hname;
  // VERIFY(gethostname(hname, 100) == 0);
  hname = "127.0.0.1";
  std::ostringstream host;
  host << hname << ":" << rlock_port;
  id = host.str();
  last_port = rlock_port;
  rpcs *rlsrpc = new rpcs(rlock_port);
  rlsrpc->reg(rlock_protocol::revoke, this, &lock_client_cache::revoke_handler);
  rlsrpc->reg(rlock_protocol::retry, this, &lock_client_cache::retry_handler);
}

lock_protocol::status
lock_client_cache::acquire(lock_protocol::lockid_t lid)
{
  int r;
  int ret = lock_protocol::OK;

  std::unique_lock<std::mutex> lck(mtx);

  auto iter = lockCache.find(lid);
  if (iter == lockCache.end()) {
    iter = lockCache.insert(std::make_pair(lid, lock_entry())).first;
  }

  while (true) {
    switch (iter->second.state)
    {
    case NONE:
      iter->second.state = ACQUIRING;
      // unlock to prevent deadlock
      lck.unlock();
      // send rpc to request lock
      ret = cl->call(lock_protocol::acquire, lid, id, r);

      // re-lock
      lck.lock();
      if (ret == lock_protocol::OK) {
        iter->second.state = LOCKED;
        return ret;
      } else if (ret == lock_protocol::RETRY) { // 否则挂起在retryQueue
        if (!iter->second.retry) {
          retryQueue.wait(lck);
        }
      } else {
        return lock_protocol::IOERR;
      }
      break;
    
    case FREE:
      iter->second.state = LOCKED;
      return ret;
    
    case LOCKED:
      waitQueue.wait(lck);
      break;
    
    case ACQUIRING:
      if (iter->second.retry) {
        // 进行retry，将标志位去掉
        iter->second.retry = false;
        lck.unlock();
        ret = cl->call(lock_protocol::acquire, lid, id, r);

        lck.lock();
        if (ret == lock_protocol::OK) {
          iter->second.state = LOCKED;
          return ret;
        } else if (ret == lock_protocol::RETRY) {
          if (!iter->second.retry) {
            retryQueue.wait(lck);
          }
        } else {
          return lock_protocol::IOERR;
        }
      } else {
        waitQueue.wait(lck);
      }
      break;
    
    case RELEASING:
      releaseQueue.wait(lck);
      break;
    }
  }
  
  return lock_protocol::OK;
}

lock_protocol::status
lock_client_cache::release(lock_protocol::lockid_t lid)
{
  int r;
  int ret = lock_protocol::OK;
  std::unique_lock<std::mutex> lck(mtx);
  
  auto iter = lockCache.find(lid);
  if (iter == lockCache.end()) {
    return lock_protocol::NOENT;
  }

  if (iter->second.revoked) {
    // server提出revoke请求
    lock_state pre_state = iter->second.state;
    iter->second.state = RELEASING;
    iter->second.revoked = false;
    lck.unlock();

    ret = cl->call(lock_protocol::release, lid, id, r);
    lck.lock();

    if (ret == lock_protocol::OK) {
      iter->second.state = NONE;
      releaseQueue.notify_all();
      return ret;
    } else {
      iter->second.revoked = true;
      iter->second.state = pre_state;
      return lock_protocol::IOERR;
    }
  } else {
    // 依然保留在缓存
    iter->second.state = FREE;
    waitQueue.notify_one();
  }

  return ret;
}

rlock_protocol::status
lock_client_cache::revoke_handler(lock_protocol::lockid_t lid, 
                                  int &)
{
  int r;
  int ret = rlock_protocol::OK;
  
  std::unique_lock<std::mutex> lck(mtx);
  
  auto iter = lockCache.find(lid);
  if (iter != lockCache.end()) {
    if (iter->second.state == FREE) {
      iter->second.state = RELEASING;
      iter->second.revoked = false;
      lck.unlock();

      ret = cl->call(lock_protocol::release, lid, id, r);
      lck.lock();

      iter->second.state = NONE;
      releaseQueue.notify_all();

    } else {
      iter->second.revoked = true;
    }
  }

  return ret;
}

rlock_protocol::status
lock_client_cache::retry_handler(lock_protocol::lockid_t lid, 
                                 int &)
{
  int ret = rlock_protocol::OK;

  std::unique_lock<std::mutex> lck(mtx);
  
  auto iter = lockCache.find(lid);
  if (iter != lockCache.end()) {
    iter->second.retry = true;
    // notify
    retryQueue.notify_one();
  }

  return ret;
}
