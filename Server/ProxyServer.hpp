#ifndef PROXY_SERVER_HPP
#define PROXY_SERVER_HPP

#include "../Cache/CacheManager.hpp"
#include "../Utils/Utils.hpp"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <thread>
#include <mutex>
#include <condition_variable>

class ProxyServer {
public:
    ProxyServer(int port);
    ~ProxyServer();

    void start();
    void handleRequest(int clientSocket, ParsedRequest *request, const std::string &tempReq);

private:
    int port_number;
    int proxy_socketId;
    HANDLE semaphore;
    CacheManager cacheManager;

    int connectRemoteServer(const std::string &host_addr, int port_num);
    int checkHTTPversion(const std::string &msg);
    int sendErrorMessage(int socket, int status_code);
};

#endif // PROXY_SERVER_HPP
