#include <iostream>
#include <cstring> // Add this include for strtok_r
#include <cstdlib> // Add this include for malloc, realloc, free
#include <cstdarg> // Add this include for va_list, va_start, va_end
#include <cstdio>  // Add this include for vfprintf
#include <cerrno>  // Add this include for errno
#include "proxy_parse.hpp"

#define DEFAULT_NHDRS 8   // initial size of header
#define MAX_REQ_LEN 65535 // max Request length
#define MIN_REQ_LEN 4     // min req length
#define DEBUG 1           // Add this line near the top with other defines

static const char *root_abs_path = "/";

using namespace std;
// debugging info
void debug(const char *format, ...)
{
    va_list args;
    if (DEBUG)
    {
        va_start(args, format);
        vfprintf(stderr, format, args);
        va_end(args);
    }
}

int ParsedHeader_set(struct ParsedRequest *PR, const char *key, const char *value)
{
    struct ParsedHeader *PH;
    ParsedHeader_remove(PR, key); // removing existing header
    if (PR->headerslen <= PR->headersused + 1)
    {
        // resizing the headers array
        PR->headerslen = PR->headerslen * 2;
        PR->headers = (struct ParsedHeader *)realloc(PR->headers, PR->headerslen * sizeof(struct ParsedHeader));
        if (!PR->headers)
            return -1;
    }
    PH = PR->headers + PR->headersused; // next header slot
    PR->headersused++;
    PH->key = (char *)malloc(strlen(key) + 1);
    // allocating memory for key and copying the key
    memcpy(PH->key, key, strlen(key) + 1);
    PH->key[strlen(key)] = '\0';

    PH->val = (char *)malloc(strlen(value) + 1);
    memcpy(PH->val, value, strlen(value));
    PH->val[strlen(value)] = '\0';

    PH->keylen = strlen(key) + 1;
    PH->valuelen = strlen(value) + 1;
    return 0;
}

// getting the parsedHeader with the specific key
struct ParsedHeader *ParsedHeader_get(struct ParsedRequest *PR, const char *key)
{
    size_t i = 0;
    struct ParsedHeader *tmp; // pointer to a structure of parsedHeader
    // return pointer to a parsedHeader structure if key found in the ParsedRequest structure
    while (PR->headersused > i)
    {
        tmp = (PR->headers) + i; // current header
        if (tmp->key && key && strcmp(tmp->key, key) == 0)
        {
            return tmp; // returning the header if key found in the headers and the key is not null
        }
        i++;
    }
    return NULL;
}

// remove the header with the specific key
int ParsedHeader_remove(struct ParsedRequest *PR, const char *key)
{
    struct ParsedHeader *tmp;
    tmp = ParsedHeader_get(PR, key);
    if (tmp == NULL)
        return -1;

    free(tmp->key);
    free(tmp->val);
    tmp->key = NULL;
    tmp->val = NULL;
    return 0;
}

// modifying the header with the given key and
// giving it to a new value
// return 1 for success and 0 for failure

int ParsedHeader_modify(struct ParsedRequest *PR, const char *key, const char *newValue)
{
    struct ParsedHeader *tmp;
    tmp = ParsedHeader_get(PR, key);
    if (tmp != NULL)
    {
        if (tmp->valuelen < strlen(newValue) + 1)
        {
            tmp->valuelen = strlen(newValue) + 1;
            tmp->val = (char *)realloc(tmp->val, tmp->valuelen);
        }
        strcpy(tmp->val, newValue);
        return 1;
    }
    return 0;
}
// initializing the headers array in ParsedRequest structure

void ParsedHeader_create(struct ParsedRequest *PR)
{
    // allocating the memory for array of headers structure
    PR->headers = (struct ParsedHeader *)malloc(sizeof(struct ParsedHeader) * DEFAULT_NHDRS);
    PR->headerslen = DEFAULT_NHDRS; // set headers len
    PR->headersused = 0;            // currently no headers are being used
}
// fn calculating the length of the line of the header line
size_t ParsedHeader_lineLen(struct ParsedHeader *PH)
{
    if (PH->key != NULL) // if key is not null
    {
        return strlen(PH->key) + strlen(PH->val) + 4;
    }
    return 0;
}

// total length of all headers in the ParsedRequest structure
size_t ParsedHeader_headersLen(struct ParsedRequest *PR)
{
    if (!PR || !PR->buff) // checking parsedRequest structure or its buffer is NULL if yes return 0
        return 0;

    size_t i = 0;
    int len = 0;
    while (PR->headersused > i) // from all used headers
    {                           // adding the length of the header line to the total length
        len += ParsedHeader_lineLen(PR->headers + i);
        i++;
    }
    len += 2; // 2 char for \r\n
    return len;
}
// now freeing the memory of the ParsedRequest structure
void ParsedHeader_destroyOne(struct ParsedHeader *PH)
{
    if (PH->key != NULL)
    {
        free(PH->key);
        PH->key = NULL;
        free(PH->val);
        PH->val = NULL;
        PH->keylen = 0;
        PH->valuelen = 0;
    }
}

int ParsedHeader_printHeaders(struct ParsedRequest *PR, char *buff, size_t len)
{
    char *current = buff;
    struct ParsedHeader *PH;
    size_t i = 0;

    if (len < ParsedHeader_headersLen(PR))
    {
        debug("buffer for printing headers too small\n");
        return -1;
    }

    while (PR->headersused > i)
    {
        PH = PR->headers + i;
        if (PH->key)
        {
            memcpy(current, PH->key, strlen(PH->key));
            memcpy(current + strlen(PH->key), ": ", 2);
            memcpy(current + strlen(PH->key) + 2, PH->val, strlen(PH->val));
            memcpy(current + strlen(PH->key) + 2 + strlen(PH->val), "\r\n", 2);
            current += strlen(PH->key) + strlen(PH->val) + 4;
        }
        i++;
    }
    memcpy(current, "\r\n", 2);
    return 0;
}

ParsedRequest *ParsedRequest_create()
{
    struct ParsedRequest *PR;
    PR = (struct ParsedRequest *)malloc(sizeof(struct ParsedRequest));
    if (PR != NULL)
    {
        ParsedHeader_create(PR);
        PR->method = NULL;
        PR->protocol = NULL;
        PR->host = NULL;
        PR->path = NULL;
        PR->query = NULL;
        PR->fragment = NULL;
        PR->body = NULL;
        PR->buff = NULL;
        PR->buff_size = 0;
        PR->port = NULL;
        PR->url = NULL;
        PR->buflen = 0;
    }
    return PR;
}
// frees all the memory of the ParsedRequest structure
void ParsedHeader_destroy(struct ParsedRequest *PR)
{
    size_t i = 0;
    while (PR->headersused > i) // all used headers
    {                           // destroying the memory of each header
        ParsedHeader_destroyOne(PR->headers + i);
        i++;
    }
    PR->headersused = 0;

    free(PR->headers);
    PR->headerslen = 0;
}

int ParsedHeader_parse(struct ParsedRequest *PR, char *line)
{ // parsing the header line and adding it to the headers array
    char *key;
    char *value;
    char *index1;
    char *index2;

    index1 = strchr(line, ':'); // first occurrence of colon
    if (index1 == NULL)
    {
        debug("No colon found\n");
        return -1;
    } // allocating memory for key including the space for null
    key = (char *)malloc((index1 - line + 1) * sizeof(char));
    memcpy(key, line, index1 - line); // copying the key from the line
    key[index1 - line] = '\0';        // null terminator to the key

    index1 += 2;
    index2 = strstr(index1, "\r\n"); // end of the line
    value = (char *)malloc((index2 - index1 + 1) * sizeof(char));
    memcpy(value, index1, (index2 - index1)); // allocating memory for the value and copying the value
    value[index2 - index1] = '\0';

    ParsedHeader_set(PR, key, value); // adding the key value pair to the headers array
    // free up space
    free(key);
    free(value);
    return 0;
}

void ParsedRequest_destroy(struct ParsedRequest *PR)
{ // freeing the memory of the ParsedRequest structure
    if (PR->buff != NULL)
    {
        free(PR->buff);
    }
    if (PR->path != NULL)
    {
        free(PR->path);
    }
    if (PR->headerslen > 0)
    {
        ParsedHeader_destroy(PR);
    }
    free(PR);
}
int ParsedRequest_unparse(struct ParsedRequest *PR, char *buff, size_t buflen)
{ // this fn recreates the entire HTTP request from the ParsedRequest structure
    if (!PR || !PR->buff)
        return -1;

    size_t tmp;
    if (ParsedRequest_printRequestLine(PR, buff, buflen, &tmp) < 0)
        return -1;
    if (ParsedHeader_printHeaders(PR, buff + tmp, buflen - tmp) < 0)
        return -1;
    return 0;
}

int ParsedRequest_unparse_headers(struct ParsedRequest *PR, char *buf, size_t buflen)
{ // recreates the headers from the ParsedRequest structure
    if (!PR || !PR->buff)
        return -1;

    if (ParsedHeader_printHeaders(PR, buf, buflen) < 0)
        return -1;
    return 0;
}

size_t ParsedRequest_totalLen(struct ParsedRequest *PR)
{
    if (!PR || !PR->buff)
        return 0;
    return ParsedRequest_requestLineLen(PR) + ParsedHeader_headersLen(PR);
}

int ParsedRequest_parse(struct ParsedRequest *parse, const char *buff, int buflen)
{
    char *full_addr;
    char *saveptr;
    char *header_end;
    char *currentHeader;
    if (parse->buff != NULL)
    {
        debug("parse obj already assigned to a request\n");
        return -1;
    }
    if (buflen < MIN_REQ_LEN || buflen > MAX_REQ_LEN)
    {
        // check buff length
        debug("buffer length out of range\n", buflen);
        return -1;
    }
    // allocating temporary buffer including null terminator
    char *tmp_buff = (char *)malloc(buflen + 1);
    memcpy(tmp_buff, buff, buflen);
    tmp_buff[buflen] = '\0';
    debug("Parsing request buffer:\n%s\n", tmp_buff); // Changed to tmp_buff to see the actual buffer content
    debug("Buffer content:\n%s\n", buff); // Add this line to debug the buffer content
    header_end = strstr(tmp_buff, "\r\n\r\n"); // end of the header
    debug("header_end: %p\n", header_end); // Print the pointer value of header_end
    if (header_end == NULL)
    {
        debug("no end of header found\n");
        free(tmp_buff);
        return -1;
    }
    debug("End of header found at position: %ld\n", header_end - tmp_buff);
    header_end += 4; // Move past the "\r\n\r\n" sequence
    // allocate buffer for the request line
    if (parse->buff == NULL)
    {
        parse->buff = (char *)malloc((header_end - tmp_buff) + 1);
        parse->buflen = (header_end - tmp_buff) + 1;
    }
    memcpy(parse->buff, tmp_buff, header_end - tmp_buff);
    parse->buff[header_end - tmp_buff] = '\0';

    // parse request line
    // parsing methods from the request line
    parse->method = strtok_s(parse->buff, " ", &saveptr);
    if (parse->method == NULL)
    {
        debug("invalid request line, no whitespace\n");
        free(tmp_buff);
        free(parse->buff);
        parse->buff = NULL;
        return -1;
    }
    debug("Method: %s\n", parse->method);
    // check the method
    const char *valid_methods[] = {"GET", "POST", "PUT", "DELETE", "HEAD", "OPTIONS", "PATCH", "CONNECT", "TRACE"};
    bool valid_method = false;
    for (const char *method : valid_methods)
    {
        if (strcmp(parse->method, method) == 0)
        {
            valid_method = true;
            break;
        }
    }
    if (!valid_method)
    {
        debug("Invalid request line, unsupported method: %s\n", parse->method);
        free(tmp_buff);
        free(parse->buff);
        parse->buff = NULL;
        return -1;
    }
    // parse the full addr from the request line
    full_addr = strtok_s(NULL, " ", &saveptr);
    if (full_addr == NULL)
    {
        debug("invalid request line, no full address\n");
        free(tmp_buff);
        free(parse->buff);
        parse->buff = NULL;
        return -1;
    }
    debug("Full address: %s\n", full_addr);
    parse->version = strtok_s(NULL, "\r\n", &saveptr); // Changed from full_addr + strlen(full_addr) + 1
    if (parse->version == NULL)
    {
        debug("invalid request line, no version\n");
        free(tmp_buff);
        free(parse->buff);
        parse->buff = NULL;
        return -1;
    }
    debug("Version: %s\n", parse->version);

    if (strncmp(parse->version, "HTTP/", 5))
    {
        debug("invalid request line, unsupported version %s\n", parse->version);
        free(tmp_buff);
        free(parse->buff);
        parse->buff = NULL;
        return -1;
    }
    // If the full address does not contain a protocol, assume HTTP
    if (strstr(full_addr, "://") == NULL)
    {
        parse->protocol = "http";
        parse->path = full_addr;
    }
    else
    {
        // parses the protocol from the full address
        parse->protocol = strtok_s(full_addr, "://", &saveptr);
        if (parse->protocol == NULL) // after ://
        {
            debug("invalid request line, missing protocol\n");
            free(tmp_buff);
            free(parse->buff);
            parse->buff = NULL;
            return -1;
        }
        debug("Protocol: %s\n", parse->protocol);
        // parses the host from the full address
        parse->host = strtok_s(NULL, "/", &saveptr);
        parse->path = strtok_s(NULL, " ", &saveptr);
    }
    if (parse->path == NULL)
    {
        // if path is null, allocating memory for the path
        int rlen = strlen(root_abs_path);
        parse->path = (char *)malloc(rlen + 1);
        memcpy(parse->path, root_abs_path, rlen + 1);
    }
    else if (parse->path[0] != '/')
    {
        // ensure path starts with a single slash
        int plen = strlen(parse->path);
        char *temp = parse->path;
        parse->path = (char *)malloc(plen + 2);
        parse->path[0] = '/';
        memcpy(parse->path + 1, temp, plen + 1);
        free(temp);
    }
    debug("Path: %s\n", parse->path);
    // parsing the headers to extract the host
    currentHeader = strstr(tmp_buff, "\r\n") + 2;
    debug("Start of headers at position: %ld\n", currentHeader - tmp_buff);
    // start of the header
    // looping through the headers
    // until the end of buff and end of headers
    while (currentHeader[0] != '\0' && !(currentHeader[0] == '\r' && currentHeader[1] == '\n'))
    {
        // parse current header
        if (ParsedHeader_parse(parse, currentHeader))
        {
            free(tmp_buff);
            free(parse->buff);
            free(parse->path);
            parse->buff = NULL;
            parse->path = NULL;
            return -1; // failed to parse header
        }
        if (DEBUG)
        {
            debug("Parsed header: %s\n", currentHeader);
        }
        // next header?
        currentHeader = strstr(currentHeader, "\r\n");
        if (currentHeader == NULL || strlen(currentHeader) < 2)
        {
            debug("no end of header found\n");
            free(tmp_buff);
            free(parse->buff);
            free(parse->path);
            parse->buff = NULL;
            parse->path = NULL;
            return -1;
        }
        currentHeader += 2;
    }
    // Ensure the end of headers is correctly identified
    if (currentHeader[0] == '\r' && currentHeader[1] == '\n')
    {
        currentHeader += 2;
    }
    else
    {
        debug("no end of header found\n");
        free(tmp_buff);
        free(parse->buff);
        free(parse->path);
        parse->buff = NULL;
        parse->path = NULL;
        return -1;
    }
    // Extract the host from the headers
    struct ParsedHeader *host_header = ParsedHeader_get(parse, "Host");
    if (host_header == NULL)
    {
        debug("invalid request line, missing host\n");
        free(tmp_buff);
        free(parse->buff);
        free(parse->path);
        parse->buff = NULL;
        parse->path = NULL;
        return -1;
    }
    parse->host = host_header->val;
    debug("Host: %s\n", parse->host);
    free(tmp_buff);
    return 0;
}

size_t ParsedRequest_requestLineLen(struct ParsedRequest *PR)
{
    if (!PR || !PR->buff)
        return 0;

    size_t len = strlen(PR->method) + 1 + strlen(PR->protocol) + 3 + strlen(PR->host) + 1 + strlen(PR->version) + 2;
    if (PR->port != NULL)
    {
        len += strlen(PR->port) + 1;
    }
    /* path is at least a slash */
    len += strlen(PR->path);
    return len;
}

int ParsedRequest_printRequestLine(struct ParsedRequest *PR, char *buff, size_t buflen, size_t *tmp)
{
    char *current = buff;

    if (buflen < ParsedRequest_requestLineLen(PR))
    {
        debug("not enough memory for first line\n");
        return -1;
    }
    memcpy(current, PR->method, strlen(PR->method));
    current += strlen(PR->method);
    current[0] = ' ';
    current += 1;

    memcpy(current, PR->protocol, strlen(PR->protocol));
    current += strlen(PR->protocol);
    memcpy(current, "://", 3);
    current += 3;
    memcpy(current, PR->host, strlen(PR->host));
    current += strlen(PR->host);
    if (PR->port != NULL)
    {
        current[0] = ':';
        current += 1;
        memcpy(current, PR->port, strlen(PR->port));
        current += strlen(PR->port);
    }
    /* path is at least a slash */
    memcpy(current, PR->path, strlen(PR->path));
    current += strlen(PR->path);

    current[0] = ' ';
    current += 1;

    memcpy(current, PR->version, strlen(PR->version));
    current += strlen(PR->version);
    memcpy(current, "\r\n", 2);
    current += 2;
    *tmp = current - buff;
    return 0;
}
