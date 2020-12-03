// the lock server implementation

#include "lock_server.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>

lock_server::lock_server():
  nacquire (0)
{
}


lock_protocol::status
lock_server::stat(int clt, lock_protocol::lockid_t lid, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
  printf("stat request from clt %d\n", clt);
  r = nacquire;
  return ret;
}


lock_protocol::status
lock_server::acquire(int clt, lock_protocol::lockid_t lid, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
  std::unique_lock<std::mutex> lck(m_mutex);
  
  auto iter = m_lockMap.find(lid);
  if (iter != m_lockMap.end()) {
    while (iter->second->m_state != lock::FREE) {
      iter->second->cv.wait(lck);
    }
    iter->second->m_state = lock::LOCKED;
  } else {
    // create new lock
    lock *new_lock = new lock(lid, lock::FREE);
    m_lockMap.insert(std::make_pair(lid, new_lock));
  }
  return ret;
}


lock_protocol::status
lock_server::release(int clt, lock_protocol::lockid_t lid, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
  std::unique_lock<std::mutex> lck(m_mutex);

  auto iter = m_lockMap.find(lid);
  if (iter != m_lockMap.end()) {
    iter->second->m_state = lock::FREE;
    iter->second->cv.notify_all();
  } else {
    ret = lock_protocol::IOERR;
  }
  return ret;
}