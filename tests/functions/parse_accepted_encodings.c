#include "../../src/http_parser.h"
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/types.h>
#include <string.h>

void test1(){
    char *t = "Accept-Encoding: gzip";
    slice_t x[100];
    ssize_t segs_size = parse_accepted_encodings(t, 21, x, 100);
    assert(segs_size == 1);
    assert(x[0].len == 4);
    assert(memcmp(x[0].p, "gzip", 4) == 0);
}

void test2(){
    char *t = "Accept-Encoding: gzip, deflate, br";
    slice_t x[100];
    ssize_t segs_size = parse_accepted_encodings(t, 34, x, 100);
    assert(segs_size == 3);
    assert(x[0].len == 4);
    assert(memcmp(x[0].p, "gzip", 4) == 0);
}


void test3(){
    char *t = "Accept-Encoding: gzip, deflate, br";
    size_t max_segs = 1;
    slice_t *x = malloc(max_segs * sizeof(size_t));
    assert(x != NULL);
    ssize_t segs_size = parse_accepted_encodings(t, 34, max_segs, 1);
    assert(segs_size == -1);
}

int main(){
    test1();
    printf("Passed test 1\n");
    test2();
    printf("Passed test 2\n");
    test3();
    printf("Passed test 3\n");
}

