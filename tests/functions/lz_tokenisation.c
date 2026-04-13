#include "../../src/gzip_compression.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>


void test1(void){
    char *test_body = "abcabcx";
    
    lz_token_t *tokenised_body;
    size_t tokenised_body_len;

    lz_tokenisation(
        test_body, strlen(test_body),
        &tokenised_body, &tokenised_body_len
    );

    lz_token_t expected[] = {
        (lz_token_t) {
            .type = TOKEN_LITERAL,
            .data.literal = 'a'
        },
        (lz_token_t) {
            .type = TOKEN_LITERAL,
            .data.literal = 'b'
        },
        (lz_token_t) {
            .type = TOKEN_LITERAL,
            .data.literal = 'c'
        },
        (lz_token_t) {
            .type = TOKEN_MATCH,
            .data.match = {
                .length = 3,
                .distance = 3,
            }
        },
        (lz_token_t) {
            .type = TOKEN_LITERAL,
            .data.literal = 'x'
        }
    };
    size_t expected_len = sizeof(expected) / sizeof(lz_token_t);

    assert(expected_len == tokenised_body_len);
    
    for (size_t i = 0; i < expected_len; i++){
        assert(expected[i].type == tokenised_body[i].type);
        if (expected[i].type == TOKEN_LITERAL){
            assert(expected[i].data.literal == tokenised_body[i].data.literal);
        }else{
            assert(expected[i].data.match.length == tokenised_body[i].data.match.length);
            assert(expected[i].data.match.distance == tokenised_body[i].data.match.distance);
        }
    }
    printf("Passed test 1\n");
}

void test2(){
    char *test_body = "aaaaa";
    
    lz_token_t *tokenised_body;
    size_t tokenised_body_len;

    lz_tokenisation(
        test_body, strlen(test_body),
        &tokenised_body, &tokenised_body_len
    );

    lz_token_t expected[] = {
        (lz_token_t) {
            .type = TOKEN_LITERAL,
            .data.literal = 'a'
        }, (lz_token_t) {
            .type = TOKEN_MATCH,
            .data.match = {
                .length = 4,
                .distance = 1,
            }
        }
    };
    size_t expected_len = sizeof(expected) / sizeof(lz_token_t);

    assert(expected_len == tokenised_body_len);
    
    for (size_t i = 0; i < expected_len; i++){
        assert(expected[i].type == tokenised_body[i].type);
        if (expected[i].type == TOKEN_LITERAL){
            assert(expected[i].data.literal == tokenised_body[i].data.literal);
        }else{
            assert(expected[i].data.match.length == tokenised_body[i].data.match.length);
            assert(expected[i].data.match.distance == tokenised_body[i].data.match.distance);
        }
    }
    printf("Passed test 2\n");
}


int main(void){
    test1();
    test2();
    return 0;
}