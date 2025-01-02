#include <iostream>
#include "proxy_parse.hpp"

void print_test_result(const char *test_name, bool passed)
{
    std::cout << test_name << ": " << (passed ? "PASSED" : "FAILED") << std::endl;
}

void test_ParsedRequest_parse()
{
    std::cout << "\n=== Test Case 1: Basic Request ===\n";
    
    // Test case 1: Valid request
    const char *request1 = "GET http://example.com/path HTTP/1.1\r\nHost: example.com\r\n\r\n";
    ParsedRequest *req = ParsedRequest_create();
    
    std::cout << "Input request: " << request1 << std::endl;
    int result = ParsedRequest_parse(req, request1, strlen(request1));
    std::cout << "Parse result: " << result << std::endl;

    if (result != 0) {
        std::cout << "FAILED: Parsing returned error code: " << result << std::endl;
    } else {
        std::cout << "Checking parsed values:\n";
        
        // Print raw pointer values for debugging
        std::cout << "Pointers - "
                  << "method: " << (void*)req->method 
                  << " protocol: " << (void*)req->protocol 
                  << " host: " << (void*)req->host 
                  << " path: " << (void*)req->path 
                  << " version: " << (void*)req->version << std::endl;

        // Safe printing of values
        std::cout << "Values:\n"
                  << "  Method: " << (req->method ? req->method : "NULL") << "\n"
                  << "  Protocol: " << (req->protocol ? req->protocol : "NULL") << "\n"
                  << "  Host: " << (req->host ? req->host : "NULL") << "\n"
                  << "  Path: " << (req->path ? req->path : "NULL") << "\n"
                  << "  Version: " << (req->version ? req->version : "NULL") << std::endl;

        // Validate results
        bool test1_passed = (req->method && strcmp(req->method, "GET") == 0) &&
                           (req->protocol && strcmp(req->protocol, "http") == 0) &&
                           (req->host && strcmp(req->host, "example.com") == 0) &&
                           (req->path && strcmp(req->path, "/path") == 0) &&
                           (req->version && strcmp(req->version, "HTTP/1.1") == 0);

        print_test_result("Basic request parsing", test1_passed);
    }
    ParsedRequest_destroy(req);

    // Test case 2: Request with port
    std::cout << "\n=== Test Case 2: Request with Port ===\n";
    const char *request2 = "GET http://example.com:8080/path HTTP/1.1\r\nHost: example.com\r\n\r\n";
    req = ParsedRequest_create();
    result = ParsedRequest_parse(req, request2, strlen(request2));

    if (result == 0 && req->port && req->host)
    {
        bool test2_passed = (strcmp(req->port, "8080") == 0 &&
                             strcmp(req->host, "example.com") == 0);
        print_test_result("Request with port parsing", test2_passed);
        std::cout << "  Host: " << req->host << std::endl;
        std::cout << "  Port: " << req->port << std::endl;
    }
    else
    {
        print_test_result("Request with port parsing", false);
    }
    ParsedRequest_destroy(req);

    // Test case 3: Request with empty path
    std::cout << "\n=== Test Case 3: Request with Empty Path ===\n";
    const char *request3 = "GET http://example.com HTTP/1.1\r\nHost: example.com\r\n\r\n";
    req = ParsedRequest_create();
    result = ParsedRequest_parse(req, request3, strlen(request3));

    bool test3_passed = (result == 0 &&
                         strcmp(req->path, "/") == 0 &&
                         strcmp(req->host, "example.com") == 0);

    print_test_result("Request with empty path parsing", test3_passed);
    if (test3_passed)
    {
        std::cout << "  Host: " << req->host << std::endl;
        std::cout << "  Path: " << req->path << std::endl;
    }
    ParsedRequest_destroy(req);
}

void test_ParsedRequest_create()
{
    ParsedRequest *req = ParsedRequest_create();
    print_test_result("ParsedRequest_create", req != NULL);
    ParsedRequest_destroy(req);
}

void test_ParsedHeader_set_get()
{
    ParsedRequest *req = ParsedRequest_create();
    const char *key = "Content-Type";
    const char *value = "text/html";

    int result = ParsedHeader_set(req, key, value);
    ParsedHeader *header = ParsedHeader_get(req, key);

    if (result == 0 && header != NULL &&
        strcmp(header->val, value) == 0)
    {
        std::cout << "ParsedHeader_set_get: PASSED" << std::endl;
    }
    else
    {
        std::cout << "ParsedHeader_set_get: FAILED" << std::endl;
    }

    ParsedRequest_destroy(req);
}

void test_ParsedHeader_remove()
{
    ParsedRequest *req = ParsedRequest_create();
    const char *key = "Content-Type";
    const char *value = "text/html";

    ParsedHeader_set(req, key, value);
    int result = ParsedHeader_remove(req, key);
    ParsedHeader *header = ParsedHeader_get(req, key);

    if (result == 0 && header == NULL)
    {
        std::cout << "ParsedHeader_remove: PASSED" << std::endl;
    }
    else
    {
        std::cout << "ParsedHeader_remove: FAILED" << std::endl;
    }

    ParsedRequest_destroy(req);
}

int main()
{
    try
    {
        std::cout << "\n=== Running All Tests ===\n";

        std::cout << "\n[1] Testing Request Creation\n";
        test_ParsedRequest_create();

        std::cout << "\n[2] Testing Request Parsing\n";
        test_ParsedRequest_parse();

        std::cout << "\n[3] Testing Header Operations\n";
        test_ParsedHeader_set_get();
        test_ParsedHeader_remove();

        std::cout << "\n=== All tests completed ===\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Test failed with error: " << e.what() << std::endl;
        return 1;
    }
}

// To compile and run the tests:
// g++ -o test_proxy_parse test_proxy_parse.cpp proxy_parse.cpp -I. && ./test_proxy_parse
