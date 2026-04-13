
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

// void huffman_encode(
//     const lz_token_t **tokenised_body, const size_t *tokenised_body_len,
    
// )

typedef struct {
    uint16_t symbol;      // DEFLATE symbol
    uint16_t base;        // smallest actual value for this symbol
    uint8_t extra_bits;   // number of extra bits after the symbol
} deflate_code_entry_t;

typedef struct {
    uint16_t symbol;
    uint8_t extra_bits;
    uint16_t extra_value;
} deflate_encoded_t;

/*
 * Length symbols: actual match length 3..258 -> symbol 257..285
 * RFC 1951, section 3.2.5
 */
static const deflate_code_entry_t LENGTH_TABLE[] = {
    {257,   3, 0},
    {258,   4, 0},
    {259,   5, 0},
    {260,   6, 0},
    {261,   7, 0},
    {262,   8, 0},
    {263,   9, 0},
    {264,  10, 0},
    {265,  11, 1},
    {266,  13, 1},
    {267,  15, 1},
    {268,  17, 1},
    {269,  19, 2},
    {270,  23, 2},
    {271,  27, 2},
    {272,  31, 2},
    {273,  35, 3},
    {274,  43, 3},
    {275,  51, 3},
    {276,  59, 3},
    {277,  67, 4},
    {278,  83, 4},
    {279,  99, 4},
    {280, 115, 4},
    {281, 131, 5},
    {282, 163, 5},
    {283, 195, 5},
    {284, 227, 5},
    {285, 258, 0}
};

/*
 * Distance symbols: actual backward distance 1..32768 -> symbol 0..29
 * RFC 1951, section 3.2.5
 */
static const deflate_code_entry_t DIST_TABLE[] = {
    { 0,     1,  0},
    { 1,     2,  0},
    { 2,     3,  0},
    { 3,     4,  0},
    { 4,     5,  1},
    { 5,     7,  1},
    { 6,     9,  2},
    { 7,    13,  2},
    { 8,    17,  3},
    { 9,    25,  3},
    {10,    33,  4},
    {11,    49,  4},
    {12,    65,  5},
    {13,    97,  5},
    {14,   129,  6},
    {15,   193,  6},
    {16,   257,  7},
    {17,   385,  7},
    {18,   513,  8},
    {19,   769,  8},
    {20,  1025,  9},
    {21,  1537,  9},
    {22,  2049, 10},
    {23,  3073, 10},
    {24,  4097, 11},
    {25,  6145, 11},
    {26,  8193, 12},
    {27, 12289, 12},
    {28, 16385, 13},
    {29, 24577, 13}
};

static deflate_encoded_t deflate_encode_length(size_t length){
    size_t table_len = sizeof(LENGTH_TABLE) / sizeof(deflate_code_entry_t);

    for (size_t i = 0; i < table_len; i++){
        deflate_code_entry_t *current_entry = LENGTH_TABLE + i;

        size_t entry_lower_bound = current_entry->base;
        size_t entry_upper_bound = current_entry->base + (1 << current_entry->extra_bits);

        if (length >= entry_lower_bound && length < entry_upper_bound){
            deflate_encoded_t encoded_length = {
                .symbol = current_entry->symbol,
                .extra_bits = current_entry->extra_bits,
                .extra_value = length - current_entry->base
            };
            return encoded_length;
        }
    }
}


static deflate_encoded_t deflate_encode_distance(size_t distance){
    size_t table_len = sizeof(DIST_TABLE) / sizeof(deflate_code_entry_t);

    for (size_t i = 0; i < table_len; i++){
        deflate_code_entry_t *current_entry = LENGTH_TABLE + i;

        size_t entry_lower_bound = current_entry->base;
        size_t entry_upper_bound = current_entry->base + (1 << current_entry->extra_bits);

        if (distance >= entry_lower_bound && distance < entry_upper_bound){
            deflate_encoded_t encoded_length = {
                .symbol = current_entry->symbol,
                .extra_bits = current_entry->extra_bits,
                .extra_value = distance - current_entry->base
            };
            return encoded_length;
        }
    }
}





