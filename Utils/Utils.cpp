#include "Utils.hpp"
#include <iostream>
#include <ctime>

std::string getCurrentTime() {
    char currentTime[50];
    time_t now = time(0);
    struct tm data = *gmtime(&now);
    strftime(currentTime, sizeof(currentTime), "%a, %d %b %Y %H:%M:%S %Z", &data);
    return std::string(currentTime);
}

void logError(const std::string &message) {
    std::cerr << message << std::endl;
}
