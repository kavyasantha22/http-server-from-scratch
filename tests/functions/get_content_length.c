#include "../../src/http_parser.h"
#include <assert.h>
#include <stddef.h>
#include <stdio.h>

int main(void){
    slice_t test1[] = {
        {(const char *)"Host: localhost", 15},
        {(const char *)"User-Agent: curl/7.64.1", 24},
        {(const char *)"Content-Length: 5", 17}
    };

    slice_t test2[] = {
        {(const char *)"Content-Length: 12345", 21}
    };

    slice_t test3[] = {
        {(const char *)"Host: localhost", 15},
        {(const char *)"Accept: */*", 11}
    };

    slice_t test4[] = {
        {(const char *)"Content-Type: application/octet-stream", 38},
        {(const char *)"Content-Length: 0", 17}
    };

    slice_t test5[] = {
        {(const char *)"Content-Length:", 15},
        {(const char *)"Content-Length: 42", 18}
    };

    assert(get_content_length(test1, 3) == 5);
    printf("Passed test 1\n");

    assert(get_content_length(test2, 1) == 12345);
    printf("Passed test 2\n");

    assert(get_content_length(test3, 2) == (size_t)-1);
    printf("Passed test 3\n");

    assert(get_content_length(test4, 2) == 0);
    printf("Passed test 4\n");

    assert(get_content_length(test5, 2) == 42);
    printf("Passed test 5\n");

    printf("All get_content_length tests passed\n");
    return 0;
}