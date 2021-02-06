// the caching lock server implementation

#include "lock_server_cache.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "lang/verify.h"
#include "handle.h"
#include "tprintf.h"


lock_server_cache::lock_server_cache()
{
}


int lock_server_cache::acquire(lock_protocol::lockid_t lid, std::string id, 
                               int &)
{
  lock_protocol::status ret = lock_protocol::OK;
  bool revoke = false;
  std::unique_lock<std::mutex> lck(mtx);

  auto iter = lockMap.find(lid);
  if (iter == lockMap.end()) {
    iter = lockMap.insert(std::make_pair(lid, lock_entry())).first;
  }

  switch (iter->second.state)
  {
  case FREE:
    iter->second.owner = id;
    iter->second.state = LOCKED;
    break;
  
  case LOCKED:
    iter->second.waitSet.insert(id);
    iter->second.state = LOCKED_AND_WAIT;
    revoke = true;
    ret = lock_protocol::RETRY;
    break;

  case LOCKED_AND_WAIT:
    iter->second.waitSet.insert(id);
    ret = lock_protocol::RETRY;
    break;
  
  case RETRYING:
    if (iter->second.waitSet.count(id)) {
      iter->second.waitSet.erase(id);
      iter->second.owner = id;
      
      if (iter->second.waitSet.size()) {
        iter->second.state = LOCKED_AND_WAIT;
        revoke = true;
      } else {
        iter->second.state = LOCKED;
      }

    } else {
      iter->second.waitSet.insert(id);
      ret = lock_protocol::RETRY;
    }

  default:
    break;
  }
  
  lck.unlock();

  if (revoke) {
    int r;
    handle(iter->second.owner).safebind()->call(rlock_protocol::revoke, lid, r);
  }

  return ret;
}

int 
lock_server_cache::release(lock_protocol::lockid_t lid, std::string id, 
         int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
  bool retry = false;
  std::string retryClient;

  std::unique_lock<std::mutex> lck(mtx);

  auto iter = lockMap.find(lid);
  if (iter == lockMap.end()) {
    return lock_protocol::IOERR;
  }
  if (iter->second.owner != id) {
    return lock_protocol::IOERR;
  }

  switch (iter->second.state)
  {
  case FREE:
    ret = lock_protocol::IOERR;
    break;
  
  case LOCKED:
    iter->second.owner = "";
    iter->second.state = FREE;
    break;
  
  case LOCKED_AND_WAIT:
    iter->second.owner = "";
    iter->second.state = RETRYING;
    retryClient = *(iter->second.waitSet.begin());
    retry = true;
    break;

  case RETRYING:
    ret = lock_protocol::IOERR;
    break;
  }
  
  lck.unlock();
  
  if (retry) {
    // send retryRPC
    handle(retryClient).safebind()->call(rlock_protocol::retry, lid, r);
  }

  return ret;
}

lock_protocol::status
lock_server_cache::stat(lock_protocol::lockid_t lid, int &r)
{
  tprintf("stat request\n");
  r = nacquire;
  return lock_protocol::OK;
}
