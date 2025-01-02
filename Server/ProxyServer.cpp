#include "ProxyServer.hpp"
#include "../proxy_parsing/proxy_parse.hpp"
#include <iostream>
#include <cstring>
#include <cstdlib>
#include <cstdio>

#define BUFFER_SIZE 8192 // Define BUFFER_SIZE with an appropriate value
#define MAX_BYTES 8192   // Define MAX_BYTES with an appropriate value

// Define the thread_args structure
struct thread_args
{
    int socket;
    ParsedRequest *request;
};

#define MAX_CLIENT 10 // Define MAX_CLIENT with an appropriate value

ProxyServer::ProxyServer(int port) : port_number(port), proxy_socketId(0), semaphore(nullptr) {}

ProxyServer::~ProxyServer()
{
    if (proxy_socketId != 0)
    {
        closesocket(proxy_socketId);
    }
    if (semaphore != nullptr)
    {
        CloseHandle(semaphore);
    }
    WSACleanup();
}

void ProxyServer::start()
{
    // Initialize Winsock
    WSADATA wsaData;
    int wsaStartup = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (wsaStartup != 0)
    {
        printf("WSAStartup failed with error: %d\n", wsaStartup);
        return;
    }

    semaphore = CreateSemaphore(NULL, MAX_CLIENT, MAX_CLIENT, NULL);
    if (semaphore == NULL)
    {
        printf("CreateSemaphore error: %d\n", GetLastError());
        return;
    }

    proxy_socketId = socket(AF_INET, SOCK_STREAM, 0);
    if (proxy_socketId == INVALID_SOCKET)
    {
        printf("Error in creating socket: %d\n", WSAGetLastError());
        WSACleanup();
        return;
    }

    int reuse = 1;
    if (setsockopt(proxy_socketId, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse, sizeof(reuse)) < 0)
    {
        perror("setsockopt(SO_REUSEADDR) failed");
        closesocket(proxy_socketId);
        WSACleanup();
        return;
    }

    struct sockaddr_in server_addr;
    memset((char *)&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port_number);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(proxy_socketId, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        printf("Error in binding: %d\n", WSAGetLastError());
        if (WSAGetLastError() == WSAEACCES)
        {
            printf("Permission denied. Try running as administrator or use a port number above 1024.\n");
        }
        closesocket(proxy_socketId);
        WSACleanup();
        return;
    }

    printf("Proxy Server is running on port %d\n", port_number);
    if (listen(proxy_socketId, MAX_CLIENT) < 0)
    {
        printf("Error in listening: %d\n", WSAGetLastError());
        closesocket(proxy_socketId);
        WSACleanup();
        return;
    }

    // Accept and handle client connections
    while (true)
    {
        struct sockaddr_in client_addr;
        int client_len = sizeof(client_addr);
        int client_socketId = accept(proxy_socketId, (struct sockaddr *)&client_addr, &client_len);
        if (client_socketId < 0)
        {
            printf("Error in accepting the client: %d\n", WSAGetLastError());
            continue;
        }

        struct thread_args *t_args = new thread_args;
        t_args->socket = client_socketId;
        t_args->request = ParsedRequest_create();
        std::thread(&ProxyServer::handleRequest, this, client_socketId, t_args->request, "").detach();
    }
}

void ProxyServer::handleRequest(int clientSocket, ParsedRequest *request, const std::string &tempReq)
{
    char *buff = (char *)malloc(sizeof(char) * MAX_BYTES);
    if (!buff)
    {
        printf("Failed to allocate buffer\n");
        return;
    }

    // printf("Handling request for host: %s, path: %s\n", request->host, request->path.c_str());

    // Build the request
    snprintf(buff, MAX_BYTES, "%s %s %s\r\nHost: %s\r\nConnection: close\r\n\r\n",
             request->method,
             request->path,
             request->version,
             request->host);

    printf("Forwarding request to remote server:\n%s\n", buff);

    int server_port = 80; // Default HTTP port
    int remoteSocketID = connectRemoteServer(request->host, server_port);

    if (remoteSocketID < 0)
    {
        printf("Error in connecting to remote server\n");
        free(buff);
        return;
    }

    int bytes_send = send(remoteSocketID, buff, strlen(buff), 0);
    // sending the request to the remote server
    if (bytes_send < 0)
    {
        printf("Error in sending the request to the remote server\n");
        return;
    }
    printf("Request sent to remote server, awaiting response...\n"); // Add this line to debug the request being sent
    memset(buff, 0, MAX_BYTES);
    char *temp_buffer = (char *)malloc(sizeof(char) * BUFFER_SIZE);
    int temp_buf_size = 0;
    int temp_buf_index = 0;
    int bytes_recv;
    while ((bytes_recv = recv(remoteSocketID, buff, MAX_BYTES - 1, 0)) > 0)
    {
        buff[bytes_recv] = '\0';                                     // Null-terminate the received data
        printf("Received response from remote server:\n%s\n", buff); // Add this line to debug the response received
        // sending the response to the client
        send(clientSocket, buff, bytes_recv, 0);
        // copying data to the buffer
        for (int i = 0; i < bytes_recv; i++)
        {
            temp_buffer[temp_buf_index] = buff[i];
            temp_buf_index++;
        }
        temp_buf_size += bytes_recv;
        temp_buffer = (char *)realloc(temp_buffer, temp_buf_size);
    }
    if (bytes_recv < 0)
    {
        perror("Error in receiving");
    }
    temp_buffer[temp_buf_index] = '\0';
    cacheManager.addCacheElement(tempReq, temp_buffer, strlen(temp_buffer)); // Corrected argument order
    printf("DONE\n");
    free(buff);
    free(temp_buffer);
}

int ProxyServer::connectRemoteServer(const std::string &host_addr, int port_num)
{
    cout << "Connecting to remote server " << host_addr << " on port " << port_num << endl; // Add this line to debug the connection details
    // remote server is where the request is to be forwarded
    int remoteSocket = socket(AF_INET, SOCK_STREAM, 0);
    // checking if the socket is created
    if (remoteSocket < 0)
    {
        printf("Error in creating remote socket\n");
        return -1;
    }
    // get host by the name or ip address provided
    struct hostent *host = gethostbyname(host_addr.c_str());
    if (host == NULL)
    {
        printf("Error in getting host by name\n");
        return -1;
    }
    // inserting ip addr and port number of host in struct server_adde
    struct sockaddr_in server_addr;
    // clearing the server address buffer
    memset((char *)&server_addr, 0, sizeof(server_addr));
    // setting the server address
    server_addr.sin_family = AF_INET; // sin_family is the address family
    // copying the ip address of the host to the server address
    server_addr.sin_port = htons(port_num); // htons converts the port number to network byte order
    memcpy((char *)&server_addr.sin_addr.s_addr, (char *)host->h_addr, host->h_length);
    /*
    bcopy is used to copy the data from one location to another
    bcopy(source, destination, length)
    */
    // connnecting to the remote server
    if (connect(remoteSocket, (struct sockaddr *)&server_addr, (socklen_t)sizeof(server_addr)) < 0)

    // connect() is used to establish a connection to the remote server
    // connect(socket, server_address, size of server address)
    {
        printf("Error in connecting to remote server: %d\n", WSAGetLastError()); // Add error code
        return -1;
    }

    // free(host_addr)
    return remoteSocket;
}

int ProxyServer::checkHTTPversion(const std::string &msg)
{
    int version = -1;

    if (msg.compare(0, 8, "HTTP/1.1") == 0)
    {
        version = 1;
    }
    else if (msg.compare(0, 8, "HTTP/1.0") == 0)
    {
        version = 1; // Handling this similar to version 1.1
    }
    else
        version = -1;

    return version;
}

int ProxyServer::sendErrorMessage(int socket, int status_code)
{
    char str[1024];
    char currentTime[50];
    time_t now = time(0);

    struct tm data = *gmtime(&now);
    strftime(currentTime, sizeof(currentTime), "%a, %d %b %Y %H:%M:%S %Z", &data);

    switch (status_code)
    {
    case 400:
        snprintf(str, sizeof(str), "HTTP/1.1 400 Bad Request\r\nContent-Length: 95\r\nConnection: keep-alive\r\nContent-Type: text/html\r\nDate: %s\r\nServer: VaibhavN/14785\r\n\r\n<HTML><HEAD><TITLE>400 Bad Request</TITLE></HEAD>\n<BODY><H1>400 Bad Rqeuest</H1>\n</BODY></HTML>", currentTime);
        printf("400 Bad Request\n");
        send(socket, str, strlen(str), 0);
        break;

    case 403:
        snprintf(str, sizeof(str), "HTTP/1.1 403 Forbidden\r\nContent-Length: 112\r\nContent-Type: text/html\r\nConnection: keep-alive\r\nDate: %s\r\nServer: VaibhavN/14785\r\n\r\n<HTML><HEAD><TITLE>403 Forbidden</TITLE></HEAD>\n<BODY><H1>403 Forbidden</H1><br>Permission Denied\n</BODY></HTML>", currentTime);
        printf("403 Forbidden\n");
        send(socket, str, strlen(str), 0);
        break;

    case 404:
        snprintf(str, sizeof(str), "HTTP/1.1 404 Not Found\r\nContent-Length: 91\r\nContent-Type: text/html\r\nConnection: keep-alive\r\nDate: %s\r\nServer: VaibhavN/14785\r\n\r\n<HTML><HEAD><TITLE>404 Not Found</TITLE></HEAD>\n<BODY><H1>404 Not Found</H1>\n</BODY></HTML>", currentTime);
        printf("404 Not Found\n");
        send(socket, str, strlen(str), 0);
        break;

    case 500:
        snprintf(str, sizeof(str), "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 115\r\nConnection: keep-alive\r\nContent-Type: text/html\r\nDate: %s\r\nServer: VaibhavN/14785\r\n\r\n<HTML><HEAD><TITLE>500 Internal Server Error</TITLE></HEAD>\n<BODY><H1>500 Internal Server Error</H1>\n</BODY></HTML>", currentTime);
        // printf("500 Internal Server Error\n");
        send(socket, str, strlen(str), 0);
        break;

    case 501:
        snprintf(str, sizeof(str), "HTTP/1.1 501 Not Implemented\r\nContent-Length: 103\r\nConnection: keep-alive\r\nContent-Type: text/html\r\nDate: %s\r\nServer: VaibhavN/14785\r\n\r\n<HTML><HEAD><TITLE>404 Not Implemented</TITLE></HEAD>\n<BODY><H1>501 Not Implemented</H1>\n</BODY></HTML>", currentTime);
        printf("501 Not Implemented\n");
        send(socket, str, strlen(str), 0);
        break;

    case 505:
        snprintf(str, sizeof(str), "HTTP/1.1 505 HTTP Version Not Supported\r\nContent-Length: 125\r\nConnection: keep-alive\r\nContent-Type: text/html\r\nDate: %s\r\nServer: VaibhavN/14785\r\n\r\n<HTML><HEAD><TITLE>505 HTTP Version Not Supported</TITLE></HEAD>\n<BODY><H1>505 HTTP Version Not Supported</H1>\n</BODY></HTML>", currentTime);
        printf("505 HTTP Version Not Supported\n");
        send(socket, str, strlen(str), 0);
        break;

    default:
        return -1;
    }
    return 1;
}
