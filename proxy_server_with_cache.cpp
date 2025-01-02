#include <iostream>
// #include <unistd.h> // Uncomment this include for close function

#ifdef DEBUG
#define debug(fmt, ...) printf(fmt, __VA_ARGS__)
#else
#define debug(fmt, ...)
#endif
#include "proxy_parsing/proxy_parse.hpp"
#include <stdio.h>
#include <windows.h>
#include <process.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <map>
#include <queue>
#include <string>
#include <ctime>
#include <vector>
#include <memory>
#include <cstring>
#include <cstdlib>

#define MIN_REQ_LEN 14   // Minimum length for a valid HTTP request
#define MAX_REQ_LEN 8192 // Maximum length for a valid HTTP request

const char *root_abs_path = "/"; // Define root_abs_path
#pragma comment(lib, "ws2_32.lib")

#define MAX_CLIENT 10
#define MAX_BYTES 10240000
#define MAX_CACHE_SIZE 1024 * 1024 * 100  // 100MB cache size
#define MAX_ELEMENT_SIZE 1024 * 1024 * 10 // 10MB max element size
#define MAX_URL_LENGTH 2048
#define BUFFER_SIZE 8192

typedef struct cache_element cache_element;
struct cache_element
{
    int len;
    char *data;
    char *url;
    time_t lru_time_back;
    time_t lru_time_track;

    cache_element *next;
};

// cache_element *find(char *url);
int add_cache_element(char *url, char *data, int size);
void remove_cache_element();
int sendErrorMessage(int socket, int status_code); // Declare the function here
int port_number = 8080;
int proxy_socketId;
HANDLE mutex;
HANDLE tid[MAX_CLIENT];
HANDLE semaphore;
std::mutex cache_lock;
cache_element *cache_head;
int cache_size;

int connectRemoteServer(char *host_addr, int port_num)
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
    struct hostent *host = gethostbyname(host_addr);
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

int checkHTTPversion(char *msg)
{
    int version = -1;

    if (strncmp(msg, "HTTP/1.1", 8) == 0)
    {
        version = 1;
    }
    else if (strncmp(msg, "HTTP/1.0", 8) == 0)
    {
        version = 1; // Handling this similar to version 1.1
    }
    else
        version = -1;

    return version;
}
int handle_request(int clientSocket, ParsedRequest *request, char *tempReq)
{
    char *buff = (char *)malloc(sizeof(char) * MAX_BYTES);
    if (!buff)
    {
        printf("Failed to allocate buffer\n");
        return -1;
    }

    printf("Handling request for host: %s, path: %s\n", request->host, request->path);

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
        return -1;
    }

    int bytes_send = send(remoteSocketID, buff, strlen(buff), 0);
    // sending the request to the remote server
    if (bytes_send < 0)
    {
        printf("Error in sending the request to the remote server\n");
        return -1;
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
    add_cache_element(tempReq, temp_buffer, strlen(temp_buffer)); // Corrected argument order
    printf("DONE\n");
    free(buff);
    free(temp_buffer);
    return 0;
}

cache_element *find(char *url)
{ // checks for url if found return pointer to a respective cache element or else return NULL
    cache_element *site = NULL;
    // checking if the cache is empty
    cache_lock.lock(); // locking the mutex
    printf("Remove Cache Lock  Aqcquired\n");
    if (cache_head != NULL)
    { // if the cache is not empty
        // checking if the url is present in the cache
        site = cache_head;
        while (site != NULL)
        {
            if (strcmp(site->url, url) == 0)
            {
                printf("LRU Time Track Before : %ld", site->lru_time_track);
                site->lru_time_track = time(NULL); // updating the lru time track
                printf("LRU Time Track After : %ld", site->lru_time_track);

                break;
            }
            site = site->next;
        }
    }
    else
    {
        printf("Cache is empty\n");
    }
    cache_lock.unlock(); // unlocking the mutex
    printf("Remove Cache Lock Released\n");
    return site;
}
void remove_cache_element()
{
    // if cache is not empty searches for the node which has least lru_time_track and deletes it
    cache_element *p;      // pointer to the node to be deleted
    cache_element *q;      // pointer to the previous node
    cache_element *temp;   // pointer to the head
    int temp_lock_val = 0; // initializing the variable
    cache_lock.lock();     // locking the mutex
    printf("Remove Cache Lock  Aqcquired value: %d\n", temp_lock_val);
    if (cache_head != NULL)
    {                                                                                         // cache !=empty
        for (q = cache_head, p = cache_head, temp = cache_head; q->next != NULL; q = q->next) // Fix the infinite loop condition
        {
            // iterate through entire cache and search for oldest time track
            if (((q->next)->lru_time_track) < (temp->lru_time_track))
            {                   // if the next node has lesser lru_time_track
                temp = q->next; // update the temp
                p = q;          // update the previous node
            }
        }
        if (temp == cache_head)
        {
            cache_head = cache_head->next;
        }
        else
        {
            p->next = temp->next;
        }

        cache_size = cache_size - (temp->len) - sizeof(cache_element);
        // cache_size is updated by subtracting the size of the node to be deleted
        strlen(temp->url) - 1; // cache size
        free(temp->data);
        free(temp->url);
        free(temp);
    }
    cache_lock.unlock(); // Unlock the mutex after removing the element
}
int add_cache_element(char *url, char *data, int size)
{
    // adds the element to cache
    cache_lock.lock();
    cout << "Add Cache Lock Aqcquired" << endl;
    int element_size = size + 1 + strlen(url) + sizeof(cache_element);
    // size of the new element which will be added to  the cache
    if (element_size > MAX_ELEMENT_SIZE)
    {
        // if element size is greate than MAX_ELEEMENT_SIZE we dpnt add the element to the cache
        cache_lock.unlock();
        cout << "Add Cache Lock Released" << endl;
        return 0;
    }
    else
    {
        // we keep removing the elements from cache untill we get enough space  ti add the element
        while (cache_size + element_size > MAX_CACHE_SIZE)
        {
            remove_cache_element();
        }
        cache_element *element = (cache_element *)malloc(sizeof(cache_element)); /// for new cache element
        // allocating mem for response tobe stored in cache element
        element->data = (char *)malloc(size + 1);
        strcpy(element->data, data);
        element->url = (char *)malloc(1 + (strlen(url) * sizeof(char))); // allocating mem for the request rto be stored in cache element (as a key)
        strcpy(element->url, url);                                       // copying the url to the cache element
        element->lru_time_track = time(NULL);
        element->next = cache_head;
        element->len = size;
        cache_head = element;
        cache_size += element_size; // updating the cache size
        cache_lock.unlock();
        printf("Add Cache lock Unlocked\n");
        return 1;
    }
    return 0;
}

struct thread_args
{
    int socket;
    ParsedRequest *request;
};

void *thread_fn(void *args)
{
    cout << "Thread function started" << endl; // Debug statement
    // thread function to handle the client request
    thread_args *t_args = (thread_args *)args;
    int socket = t_args->socket;
    ParsedRequest *request = t_args->request;
    char *request_str = NULL; // Declare request_str here
    if (socket != -1)
    {
        cout << "Waiting for semaphore" << endl; // Debug statement
        WaitForSingleObject(semaphore, INFINITE);
        LONG p; // socket id
        if (!ReleaseSemaphore(semaphore, 0, &p))
        {
            printf("Error getting semaphore value: %d\n", GetLastError());
        }
        p = MAX_CLIENT - p; // Adjusting the value to match the expected behavior
        printf("Semaphore Value: %d\n", p);
    }
    int bytes_send_client, len;
    char *tempReq = NULL;
    // bytes transfered
    char *buffer = (char *)calloc(MAX_BYTES, sizeof(char));
    // buff for client
    if (socket != -1)
    {
        cout << "Receiving request from client" << endl; // Debug statement
        memset(buffer, 0, MAX_BYTES);
        bytes_send_client = recv(socket, buffer, MAX_BYTES, 0); // recieving the request of client by proxy server
        if (bytes_send_client < 0)
        {
            perror("Error in recieving the request\n");
            return NULL;
        }
    }
    else
    {
        // Handle the request string directly
        request_str = t_args->request->buff;
        strcpy(buffer, request_str);
        bytes_send_client = strlen(buffer);
    }
    debug("Received buffer:\n%s\n", buffer); // Add this line to debug the buffer content
    while (bytes_send_client > 0)
    { // while bytes are being recived
        len = strlen(buffer);
        // loop untill finding "\r\n\r\n" in the buffer
        if (strstr(buffer, "\r\n\r\n") == NULL)
        {
            if (socket != -1)
            {
                bytes_send_client = recv(socket, buffer + len, MAX_BYTES - len, 0);
            }
            else
            {
                break;
            }
        }
        else
            break;
    }
    tempReq = (char *)malloc(strlen(buffer) * sizeof(char) + 1);
    // tempReq, buffer both store the http req sent by client
    for (int i = 0; i < strlen(buffer); i++)
    {
        tempReq[i] = buffer[i]; // copying buffer to tempReq
    }
    tempReq[strlen(buffer)] = '\0'; // Null-terminate tempReq
    // checking for the request in the cache
    struct cache_element *temp = find(tempReq);
    if (temp != NULL)
    {
        // request found in the cache, so sendig the response to client//from proxys cache
        int size = temp->len / sizeof(char); // size of the response
        int pos = 0;
        char response[MAX_BYTES];
        while (pos < size)
        {
            memset(response, 0, MAX_BYTES);
            // sending the response to the client
            for (int i = 0; i < MAX_BYTES && pos < size; i++)
            {
                response[i] = temp->data[pos];
                pos++;
            }
            send(socket, response, MAX_BYTES, 0);
        }
        cout << "Data retrieved from cache\n";
        cout << "\n\n"
             << response << "\n\n"
             << temp->data;
    }
    else if (bytes_send_client > 0)
    { // if req not found
        len = strlen(buffer);
        cout << "Request length: " << len << endl; // Debug statement
        if (ParsedRequest_parse(request, buffer, len) < 0)
        {
            cout << "Parsed failed" << endl;
        }
        else
        {
            // checking for the request type
            memset(buffer, 0, MAX_BYTES);
            if (!strcmp(request->method, "GET"))
            {
                if (request->host && request->path && (checkHTTPversion(request->version) == 1))
                {
                    // if the request is valid  then handle the request and send the response to the client
                    bytes_send_client = handle_request(socket, request, tempReq);
                    if (bytes_send_client == -1)
                        sendErrorMessage(socket, 500);
                }
                else
                    sendErrorMessage(socket, 400);
            }
            else
                cout << "Method not supported" << endl;
        }
    }
    if (bytes_send_client < 0)
    {
        cout << "Error in recieving the request" << endl;
    }
    else if (bytes_send_client == 0)
    {
        cout << "Client disconnected" << endl;
    }
    if (socket != -1)
    {
        shutdown(socket, SD_BOTH);
        closesocket(socket);
    }
    free(buffer);
    if (socket != -1)
    {
        ReleaseSemaphore(semaphore, 1, NULL);
        LONG p;
        if (!ReleaseSemaphore(semaphore, 0, &p))
        {
            printf("Error getting semaphore value: %d\n", GetLastError());
        }
        p = MAX_CLIENT - p; // Adjusting the value to match the expected behavior
        cout << "Semaphore Value: " << p << endl;
    }
    free(tempReq);
    free(t_args);
    cout << "Thread function ended" << endl; // Debug statement
    return NULL;
}

int main(int argc, char *argv[])
{
    int client_socketId, client_len;
    struct sockaddr_in server_addr, client_addr;

    // Initialize Winsock
    WSADATA wsaData;
    int wsaStartup = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (wsaStartup != 0)
    {
        printf("WSAStartup failed with error: %d\n", wsaStartup);
        return 1;
    }

    semaphore = CreateSemaphore(NULL, MAX_CLIENT, MAX_CLIENT, NULL);
    if (semaphore == NULL)
    {
        printf("CreateSemaphore error: %d\n", GetLastError());
        return 1;
    }
    if (argc < 2)
    {
        printf("Usage: %s <port_number> [request]\n", argv[0]);
        return 1;
    }
    port_number = atoi(argv[1]);
    const char *request_str = NULL;
    if (argc == 3)
    {
        // Directly assign the request string for testing purposes
        request_str = "GET / HTTP/1.1\r\nHost: google.com\r\n\r\n";
    }
    printf("Setting Proxy Server Port : %d\n", port_number);

    proxy_socketId = socket(AF_INET, SOCK_STREAM, 0);
    if (proxy_socketId == INVALID_SOCKET)
    {
        printf("Error in creating socket: %d\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }
    printf("settring proxy server port: %d\n", port_number);

    int reuse = 1;
    if (setsockopt(proxy_socketId, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse, sizeof(reuse)) < 0)
    {
        perror("setsockopt(SO_REUSEADDR) failed");
        closesocket(proxy_socketId);
        WSACleanup();
        return 1;
    }

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
        return 1;
    }
    printf("Proxy Server is running on port %d\n", port_number);
    int listen_status = listen(proxy_socketId, MAX_CLIENT);
    if (listen_status < 0)
    {
        printf("Error in listening: %d\n", WSAGetLastError());
        closesocket(proxy_socketId);
        WSACleanup();
        return 1;
    }

    if (request_str != NULL)
    {
        cout << "Request string: " << request_str << endl;
        struct thread_args *t_args = (struct thread_args *)malloc(sizeof(struct thread_args));
        t_args->socket = -1;
        t_args->request = ParsedRequest_create();

        // Parse the request string directly
        int parse_result = ParsedRequest_parse(t_args->request, request_str, strlen(request_str));
        if (parse_result < 0)
        {
            printf("Failed to parse request: %d\n", parse_result);
            ParsedRequest_destroy(t_args->request);
            free(t_args);
            return 1;
        }

        // Set required fields for direct request
        t_args->request->method = strdup("GET");
        t_args->request->path = strdup("/");
        t_args->request->version = strdup("HTTP/1.1");
        t_args->request->host = strdup("google.com");

        // Process the request
        thread_fn((void *)t_args);

        ParsedRequest_destroy(t_args->request);
        free(t_args);
    }
    else
    {
        int i = 0;
        int Connected_socketId[MAX_CLIENT];
        // accepting the client
        while (1)
        {
            memset((char *)&client_addr, 0, sizeof(client_addr)); // clearing the client address buffer
            client_len = sizeof(client_addr);                     // getting the client address length
            // accepting the client
            client_socketId = accept(proxy_socketId, (struct sockaddr *)&client_addr, &client_len);
            // checking if the client is connected
            if (client_socketId < 0)
            {
                printf("Error in accepting the client: %d\n", WSAGetLastError());
                continue;
            }
            printf("Client %d is connected\n", i + 1);
            Connected_socketId[i] = client_socketId;
            // getting ip addr anmd port number of the client
            //
            struct sockaddr_in *client_ip = (struct sockaddr_in *)&client_addr; // getting the client ip address

            struct in_addr ip_addr = client_ip->sin_addr;
            char str[INET_ADDRSTRLEN];                          // buffer to store the ip address
            inet_ntop(AF_INET, &ip_addr, str, INET_ADDRSTRLEN); // converting the ip address to string
            printf("Client IP Address: %s\n", str);
            printf("Client Port Number: %d\n", ntohs(client_ip->sin_port));
            struct thread_args *t_args = (struct thread_args *)malloc(sizeof(struct thread_args));
            t_args->socket = client_socketId;
            t_args->request = ParsedRequest_create(); // Initialize request here
            tid[i] = (HANDLE)_beginthreadex(NULL, 0, (_beginthreadex_proc_type)thread_fn, (void *)t_args, 0, NULL);
            i++;
        }
    }
    closesocket(proxy_socketId);
    WSACleanup();
    return 0;
}
int sendErrorMessage(int socket, int status_code)
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
