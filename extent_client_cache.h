#ifndef extent_client_cache_h
#define extent_client_cache_h

#include "extent_client.h"
#include <mutex>
#include <unordered_map>

class extent_client_cache : public extent_client {
public:
    extent_client_cache(std::string dst);
    extent_protocol::status get(extent_protocol::extentid_t eid,
                                std::string &buf);
    extent_protocol::status getattr(extent_protocol::extentid_t eid,
                                    extent_protocol::attr &a);
    extent_protocol::status put(extent_protocol::extentid_t eid, std::string buf);
    extent_protocol::status remove(extent_protocol::extentid_t eid);
    extent_protocol::status flush(extent_protocol::extentid_t eid);

private:
    enum extent_state {
        NONE,
        CACHED,     // 表示缓存未修改
        UPDATED,    // 缓存已更新
        REMOVED     // 缓存已删除
    };

    struct extent_entry {
        std::string data;
        extent_protocol::attr attr;
        extent_state state;

        extent_entry() : state(NONE) {}
    };
    
    std::unordered_map<extent_protocol::extentid_t, extent_entry> extentCache;
    std::mutex mtx;
};

#endif