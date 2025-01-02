#include "CacheManager.hpp"
#include <iostream>

#define MAX_ELEMENT_SIZE 1024  // Define a suitable value for MAX_ELEMENT_SIZE
#define MAX_CACHE_SIZE 8192    // Define a suitable value for MAX_CACHE_SIZE

CacheManager::CacheManager() : cache_head(nullptr), cache_size(0) {}

CacheManager::~CacheManager() {
    while (cache_head != nullptr) {
        CacheElement *temp = cache_head;
        cache_head = cache_head->next;
        delete temp;
    }
}

CacheElement* CacheManager::find(const std::string &url) {
    std::lock_guard<std::mutex> lock(cache_lock);
    CacheElement *site = cache_head;
    while (site != nullptr) {
        if (url == site->url) {
            site->lru_time_track = time(NULL);
            return site;
        }
        site = site->next;
    }
    return nullptr;
}

int CacheManager::addCacheElement(const std::string &url, const std::string &data, int size) {
    std::lock_guard<std::mutex> lock(cache_lock);
    int element_size = size + url.size() + sizeof(CacheElement);
    if (element_size > MAX_ELEMENT_SIZE) {
        return 0;
    }
    while (cache_size + element_size > MAX_CACHE_SIZE) {
        removeCacheElement();
    }
    CacheElement *element = new CacheElement(url, data, size);
    element->next = cache_head;
    cache_head = element;
    cache_size += element_size;
    return 1;
}

void CacheManager::removeCacheElement() {
    std::lock_guard<std::mutex> lock(cache_lock);
    if (cache_head == nullptr) return;
    CacheElement *p = cache_head;
    CacheElement *q = cache_head;
    CacheElement *temp = cache_head;
    while (q->next != nullptr) {
        if (q->next->lru_time_track < temp->lru_time_track) {
            temp = q->next;
            p = q;
        }
        q = q->next;
    }
    if (temp == cache_head) {
        cache_head = cache_head->next;
    } else {
        p->next = temp->next;
    }
    cache_size -= temp->len + sizeof(CacheElement);
    delete temp;
}
