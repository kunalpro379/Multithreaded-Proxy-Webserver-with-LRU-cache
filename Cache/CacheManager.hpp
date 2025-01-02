#ifndef CACHE_MANAGER_HPP
#define CACHE_MANAGER_HPP

#include "CacheElement.hpp"
#include <mutex>

class CacheManager {
public:
    CacheManager();
    ~CacheManager();

    CacheElement* find(const std::string &url);
    int addCacheElement(const std::string &url, const std::string &data, int size);
    void removeCacheElement();

private:
    CacheElement *cache_head;
    int cache_size;
    std::mutex cache_lock;
};

#endif // CACHE_MANAGER_HPP
