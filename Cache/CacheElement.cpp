#include "CacheElement.hpp"
#include <cstring>

CacheElement::CacheElement(const std::string &url, const std::string &data, int len)
    : len(len), lru_time_back(0), lru_time_track(time(NULL)), next(nullptr) {
    this->data = new char[data.size() + 1];
    std::strcpy(this->data, data.c_str());
    this->url = new char[url.size() + 1];
    std::strcpy(this->url, url.c_str());
}

CacheElement::~CacheElement() {
    delete[] data;
    delete[] url;
}
