#include "../../src/http_parser.h"
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

int main(void){
    char no_crlf[] = "this doesn't contain crlf crlf";
    char one_crlf[] = "this contains one crlf \r\n";
    char two_crlf[] = "this contains two crlf \r\n\r\n";
    char two_crlf_middle[] = "this contains \r\n\r\n two crlf";

    assert(contains_header_end(no_crlf, no_crlf + sizeof(no_crlf)) == -1);
    printf("Passed test 1\n");
    assert(contains_header_end(one_crlf, one_crlf + sizeof(one_crlf)) == -1);
    printf("Passed test 2\n");
    assert(contains_header_end(two_crlf, two_crlf + sizeof(two_crlf)) >= 0);
    printf("Passed test 3\n");
    assert(contains_header_end(two_crlf_middle, two_crlf_middle + sizeof(two_crlf_middle)) >= 0);
    printf("Passed test 4\n");
}