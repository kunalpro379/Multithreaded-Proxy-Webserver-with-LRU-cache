#ifndef PROXY_PARSE_HPP
#define PROXY_PARSE_HPP

#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <thread>
#include <string.h>
#include <vector>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <windows.h>
#include <cstdarg>
#include <cstring>
#include <ctype.h>
#include <errno.h>

using namespace std;

#define DEBUG 1

// single http header "Content-Type: application/json"
struct ParsedHeader
{
    char *key;
    size_t keylen;
    char *val;
    size_t valuelen;
};

// Complete HTTP request
struct ParsedRequest
{
    char *method;
    const char *protocol; // Change protocol to const char*
    char *host;
    char *path;
    char *query;
    char *fragment;
    char *body;
    char *buff;
    size_t buff_size;
    char *port;
    char *url;
    char *version;
    struct ParsedHeader *headers; // Array of headers
    size_t buflen;
    size_t headersused; // Number of headers currently in use
    size_t headerslen;  // Total allocated header space
};

// Function declarations

// Function to create an empty parsed obj
// dynamically allocates the memory for ParsedReq
ParsedRequest *ParsedRequest_create();

// Function to free the memory of ParsedReq obj
void ParsedRequest_destroy(ParsedRequest *PR);

// Function to parse an HTTP request buffer into the ParsedReq Object
// It extracts the method, Protocol, host, etc
// 0 if success, -1 if error
int ParsedRequest_parse(ParsedRequest *parse, const char *buff, int buflen);

// Function to rebuild the HTTP req as a string from a ParsedReq Object
// String includes the request line headers and trailing
// 0 if success, -1 if error
int ParsedRequest_unparse(ParsedRequest *parse, char *buf, size_t buflen);

// Function to rebuild only the headers section of HTTP req into string
// Excludes the req line includes trailing if headers exist
// 0 if success, -1 if error
int ParsedRequest_unparse_headers(ParsedRequest *parse, char *buf, size_t buflen);

// Function to get the total len of HTTP req including headers and body
size_t ParsedRequest_totalLen(ParsedRequest *PR);

// Function to get the length of just the headers section, including trailing "\r\n"
size_t ParsedHeader_headersLen(ParsedRequest *PR);

// Function to set (add or update) a header in the ParsedRequest
// If the key exists, it updates the value; otherwise, it adds a new header
int ParsedHeader_set(ParsedRequest *PR, const char *key, const char *value);

// Function to retrieve a specific header (key) from the ParsedRequest
// Returns a pointer to the ParsedHeader object if the key exists; otherwise, returns nullptr
ParsedHeader *ParsedHeader_get(ParsedRequest *PR, const char *key);

// Function to remove a header (key) from the ParsedRequest
// Returns 0 on success, -1 if the key is not found
int ParsedHeader_remove(ParsedRequest *PR, const char *key);

// Function to modify a header in the ParsedRequest
int ParsedHeader_modify(ParsedRequest *PR, const char *key, const char *newValue);

// Function to create headers in the ParsedRequest
void ParsedHeader_create(ParsedRequest *PR);

// Function to parse a single header line
int ParsedHeader_parse(ParsedRequest *PR, char *line);

// Function to destroy a single header
void ParsedHeader_destroyOne(ParsedHeader *PH);

// Additional helper function declarations
size_t ParsedRequest_requestLineLen(struct ParsedRequest *PR);
int ParsedRequest_printRequestLine(struct ParsedRequest *PR, char *buff, size_t buflen, size_t *tmp);
void ParsedHeader_destroy(struct ParsedRequest *PR);
int ParsedHeader_printHeaders(struct ParsedRequest *PR, char *buff, size_t len);

#endif