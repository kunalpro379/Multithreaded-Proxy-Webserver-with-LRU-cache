#ifndef CACHE_ELEMENT_HPP
#define CACHE_ELEMENT_HPP

#include <ctime>
#include <string>

class CacheElement {
public:
    int len;
    char *data;
    char *url;
    time_t lru_time_back;
    time_t lru_time_track;
    CacheElement *next;

    CacheElement(const std::string &url, const std::string &data, int len);
    ~CacheElement();
};

#endif // CACHE_ELEMENT_HPP
