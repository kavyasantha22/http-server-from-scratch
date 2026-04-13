
#include <sys/types.h>
#include <stdint.h>
#include <stdlib.h>

#include "http_compression.h"

#define GZIP_WINDOW_SIZE 32768


int build_gzip_encoded_body(
    const char *body, size_t body_len,
    char **encoded_body, size_t* encoded_body_len
){
    const uint8_t header_bytes[] = {
        0x1f, 0x8b, 0x08, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0xff
    };

    lz_token_t *tokenised_body;
    size_t tokenised_body_len;
    lz_tokenisation(
        body, body_len,
        &tokenised_body, &tokenised_body_len
    );
}

int lz_tokenisation(
    const char *body, size_t body_len,
    lz_token_t **tokenised_body, size_t *tokenised_body_len
){
    *tokenised_body = malloc(body_len * sizeof(lz_token_t));
    *tokenised_body_len = 0;
    
    size_t cur_body_idx = 0;

    while (cur_body_idx < body_len){
        size_t left_bound = 0;
        if (cur_body_idx > GZIP_WINDOW_SIZE) left_bound = cur_body_idx - GZIP_WINDOW_SIZE; 

        size_t max_matched_length = 0;
        size_t max_matched_start_idx;
        size_t matched_idx = left_bound;
    
        while (matched_idx < cur_body_idx){
            size_t matched_length = 0;
            while (body[matched_idx + matched_length] == body[cur_body_idx + matched_length]){
                if (
                    matched_length >= 258 || 
                    cur_body_idx + matched_length >= body_len 
                ) break;
                else matched_length++;
            }
            if (matched_length > max_matched_length){
                max_matched_length = matched_length;
                max_matched_start_idx = matched_idx;
            }
            matched_idx++;
        }

        if (max_matched_length >= 3){
            (*tokenised_body)[*tokenised_body_len] = (lz_token_t) {
                .type = TOKEN_MATCH,
                .data.match = {
                    .length = max_matched_length,
                    .distance = cur_body_idx - max_matched_start_idx,
                }
            };
            cur_body_idx += max_matched_length;
            (*tokenised_body_len)++;
        }else{
            (*tokenised_body)[*tokenised_body_len] = (lz_token_t) {
                .type = TOKEN_LITERAL,
                .data.literal = body[cur_body_idx]
            };
            cur_body_idx++;
            (*tokenised_body_len)++;
        }
    }  
}

// int huffman_encode(
//     const lz_token_t* tokenised_body, const size_t tokenised_body_len,
//     encoded_buf
// ){

// }

// int huffman_encode_length(){

// }

// int huffman_enco


