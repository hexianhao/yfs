// RPC stubs for clients to talk to extent_server

#include "extent_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <time.h>

// The calls assume that the caller holds a lock on the extent

extent_client::extent_client(std::string dst)
{
  sockaddr_in dstsock;
  make_sockaddr(dst.c_str(), &dstsock);
  cl = new rpcc(dstsock);
  if (cl->bind() != 0) {
    printf("extent_client: bind failed\n");
  }
}

extent_protocol::status
extent_client::get(extent_protocol::extentid_t eid, std::string &buf)
{
    extent_protocol::status ret = extent_protocol::OK;
    std::lock_guard<std::mutex> lg(mtx);
    
    auto iter = extentCache.find(eid);
    if (iter != extentCache.end()) {
        switch (iter->second.state)
        {
        case NONE:
            ret = cl->call(extent_protocol::get, eid, buf);
            if (ret == extent_protocol::OK) {
                // 更新缓存
                iter->second.data = buf;
                iter->second.state = CACHED;
                iter->second.attr.atime = time(nullptr);
                iter->second.attr.size = buf.size();
            }
            break;
        
        case CACHED:
        case UPDATED:
            buf = iter->second.data;
            iter->second.attr.atime = time(nullptr);
            break;

        case REMOVED:
            ret = extent_protocol::NOENT;
            break;
        }
    } else {
        ret = cl->call(extent_protocol::get, eid, buf);
        if (ret == extent_protocol::OK) {
            extentCache[eid].data = buf;
            extentCache[eid].state = CACHED;
            extentCache[eid].attr.atime = time(nullptr);
            extentCache[eid].attr.ctime = 0;
            extentCache[eid].attr.mtime = 0;
            extentCache[eid].attr.size = buf.size();
        }
    }
    return ret;
}

extent_protocol::status
extent_client::getattr(extent_protocol::extentid_t eid, extent_protocol::attr &a)
{
    extent_protocol::status ret = extent_protocol::OK;
    std::lock_guard<std::mutex> lg(mtx);

    auto iter = extentCache.find(eid);
    if (iter != extentCache.end()) {
        switch (iter->second.state)
        {
        case NONE:
        case CACHED:
        case UPDATED:
            a = iter->second.attr;
            break;
        
        case REMOVED:
            ret = extent_protocol::NOENT;
            break;
        }
    } else {
        extentCache[eid].state = NONE;
        ret = cl->call(extent_protocol::getattr, eid, a);
        if (ret == extent_protocol::OK) {
            extentCache[eid].attr.atime = a.atime;
            extentCache[eid].attr.ctime = a.ctime;
            extentCache[eid].attr.mtime = a.mtime;
            extentCache[eid].attr.size = a.size;
        }
    }

    return ret;
}

extent_protocol::status
extent_client::put(extent_protocol::extentid_t eid, std::string buf)
{
    extent_protocol::status ret = extent_protocol::OK;
    std::lock_guard<std::mutex> lg(mtx);

    auto iter = extentCache.find(eid);
    if (iter != extentCache.end()) {
        switch (iter->second.state)
        {
        case NONE:
        case CACHED:
        case UPDATED:
            iter->second.data = buf;
            iter->second.state = UPDATED;
            iter->second.attr.mtime = time(nullptr);
            iter->second.attr.ctime = time(nullptr);
            iter->second.attr.size = buf.size();
            break;
        
        case REMOVED:
            ret = extent_protocol::NOENT;
            break;
        }
    } else {
        extentCache[eid].data = buf;
        extentCache[eid].state = UPDATED;
        extentCache[eid].attr.mtime = time(nullptr);
        extentCache[eid].attr.ctime = time(nullptr);
        extentCache[eid].attr.size = buf.size();
    }
}

extent_protocol::status
extent_client::remove(extent_protocol::extentid_t eid)
{
    extent_protocol::status ret = extent_protocol::OK;
    std::lock_guard<std::mutex> lg(mtx);

    auto iter = extentCache.find(eid);
    if (iter != extentCache.end()) {
        switch (iter->second.state)
        {
        case NONE:
        case CACHED:
        case UPDATED:
            iter->second.state = REMOVED;
            break;
        
        case REMOVED:
            ret = extent_protocol::NOENT;
            break;
        }
    } else {
        ret = extent_protocol::NOENT;
    }

    return ret;
}

extent_protocol::status
extent_client::flush(extent_protocol::extentid_t eid)
{
    int r;
    extent_protocol::status ret = extent_protocol::OK;
    std::lock_guard<std::mutex> lg(mtx);

    auto iter = extentCache.find(eid);
    if (iter != extentCache.end()) {
        switch (iter->second.state)
        {
        case NONE:
            ret = extent_protocol::NOENT;
            break;
        
        case CACHED:
            break;
        
        case UPDATED:
            ret = cl->call(extent_protocol::put, eid, iter->second.data, r);
            break;

        case REMOVED:
            ret = cl->call(extent_protocol::remove, eid, r);
            break;
        }
        extentCache.erase(eid);
    } else {
        ret = extent_protocol::NOENT;
    }

    return ret;
}