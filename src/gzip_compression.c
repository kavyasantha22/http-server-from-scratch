
#include <sys/types.h>
#include <stdint.h>
#include <stdlib.h>

#include "gzip_compression.h"
#include "bit_writer.h"

#define GZIP_WINDOW_SIZE 32768
#define FIXED_HUFFMAN_BTYPE 1


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

static uint32_t CRC32_TABLE[256];

static uint16_t FIXED_LITERAL_LENGTH_CODE[288];
static uint8_t FIXED_LITERAL_LENGTH_NBITS[288];
static uint16_t FIXED_DISTANCE_CODE[32];
static uint8_t FIXED_DIST_NBITS[32];

void huffman_encode(
    const lz_token_t *tokenised_body, const size_t tokenised_body_len,
    bit_writer_t *bw
);

uint32_t crc32(const char *body, const size_t body_size);


static deflate_encoded_t deflate_encode_length(size_t length){
    size_t table_len = sizeof(LENGTH_TABLE) / sizeof(deflate_code_entry_t);

    for (size_t i = 0; i < table_len; i++){
        const deflate_code_entry_t *current_entry = LENGTH_TABLE + i;

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
    return (deflate_encoded_t){0};
}


static deflate_encoded_t deflate_encode_distance(size_t distance){
    size_t table_len = sizeof(DIST_TABLE) / sizeof(deflate_code_entry_t);

    for (size_t i = 0; i < table_len; i++){
        const deflate_code_entry_t *current_entry = DIST_TABLE + i;

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
    return (deflate_encoded_t){0};
}


static void init_fixed_huffman_tables(void) {
    // literal/length symbols 0..143: 8 bits, codes 00110000..10111111
    for (uint16_t s = 0; s <= 143; s++) {
        FIXED_LITERAL_LENGTH_CODE[s] = (uint16_t)(0x30 + s);
        FIXED_LITERAL_LENGTH_NBITS[s] = 8;
    }

    // literal/length symbols 144..255: 9 bits, codes 110010000..111111111
    for (uint16_t s = 144; s <= 255; s++) {
        FIXED_LITERAL_LENGTH_CODE[s] = (uint16_t)(0x190 + (s - 144));
        FIXED_LITERAL_LENGTH_NBITS[s] = 9;
    }

    // literal/length symbols 256..279: 7 bits, codes 0000000..0010111
    for (uint16_t s = 256; s <= 279; s++) {
        FIXED_LITERAL_LENGTH_CODE[s] = (uint16_t)(s - 256);
        FIXED_LITERAL_LENGTH_NBITS[s] = 7;
    }

    // literal/length symbols 280..287: 8 bits, codes 11000000..11000111
    for (uint16_t s = 280; s <= 287; s++) {
        FIXED_LITERAL_LENGTH_CODE[s] = (uint16_t)(0xC0 + (s - 280));
        FIXED_LITERAL_LENGTH_NBITS[s] = 8;
    }

    // distance symbols 0..31: 5 bits, codes 00000..11111
    for (uint16_t s = 0; s <= 31; s++) {
        FIXED_DISTANCE_CODE[s] = s;
        FIXED_DIST_NBITS[s] = 5;
    }
}


static void init_crc32_table(void) {
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t crc = i;
        for (int j = 0; j < 8; j++) {
            if (crc & 1)
                crc = (crc >> 1) ^ 0xEDB88320;
            else
                crc >>= 1;
        }
        CRC32_TABLE[i] = crc;
    }
}


static uint16_t reverse_bits(uint16_t data, uint8_t nbits){
    uint16_t reversed_data = 0;
    for (uint8_t i = 0; i < nbits; i++){
        reversed_data = (reversed_data << 1) | (data & 1);
        data >>= 1;
    }
    return reversed_data;
}


void build_gzip_encoded_body(
    const char *body, size_t body_len,
    char **encoded_body, size_t* encoded_body_len
){
    init_fixed_huffman_tables();
    init_crc32_table();
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

    size_t buf_capacity = 2 * body_len + 64;
    uint8_t *buf = malloc(buf_capacity);
    bit_writer_t bw;
    bw_init(&bw, buf, buf_capacity);

    for (size_t i = 0; i < 10; i++){
        bw.buf[bw.pos++] = header_bytes[i];
    }

    huffman_encode(
        tokenised_body, tokenised_body_len,
        &bw
    );

    bw_flush(&bw);

    uint32_t body_crc = crc32(body, body_len);
    uint32_t isize = (uint32_t)body_len;  

    for (size_t i = 0; i < 4; i++){
        bw.buf[bw.pos++] = (uint8_t)(body_crc & 0xFF);
        body_crc >>= 8;
    }

    for (size_t i = 0; i < 4; i++){
        bw.buf[bw.pos++] = (uint8_t)(isize & 0xFF);
        isize >>= 8;
    }

    *encoded_body = (char *)bw.buf;
    *encoded_body_len = bw_bytes_written(&bw);

    free(tokenised_body);
}


void lz_tokenisation(
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

            while (
                cur_body_idx + matched_length < body_len && 
                body[matched_idx + matched_length] == body[cur_body_idx + matched_length]
            ){
                if (matched_length >= 258) break;
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


void huffman_encode(
    const lz_token_t *tokenised_body, const size_t tokenised_body_len,
    bit_writer_t *bw
){
    bw_write_bits(bw, 1, 1);
    bw_write_bits(bw, FIXED_HUFFMAN_BTYPE, 2);

    for (size_t i = 0; i < tokenised_body_len; i++){
        const lz_token_t *current_token = tokenised_body + i;
        if (current_token->type == TOKEN_LITERAL){
            deflate_encoded_t deflated_literal = {
                .symbol = current_token->data.literal,
                .extra_bits = 0,
                .extra_value = 0
            };

            uint16_t huffman_literal_code = FIXED_LITERAL_LENGTH_CODE[deflated_literal.symbol];
            uint8_t huffman_literal_nbits = FIXED_LITERAL_LENGTH_NBITS[deflated_literal.symbol];

            bw_write_bits(
                bw,
                reverse_bits(huffman_literal_code, huffman_literal_nbits),
                huffman_literal_nbits
            );
            
        }else{
            deflate_encoded_t deflated_length = deflate_encode_length(current_token->data.match.length);
            uint16_t huffman_length_code = FIXED_LITERAL_LENGTH_CODE[deflated_length.symbol];
            uint16_t huffman_length_nbits = FIXED_LITERAL_LENGTH_NBITS[deflated_length.symbol];

            bw_write_bits(
                bw,
                reverse_bits(huffman_length_code, huffman_length_nbits),
                huffman_length_nbits
            );

            bw_write_bits(
                bw,
                deflated_length.extra_value,
                deflated_length.extra_bits
            );

            deflate_encoded_t deflated_distance = deflate_encode_distance(current_token->data.match.distance);
            uint16_t huffman_distance_code = FIXED_DISTANCE_CODE[deflated_distance.symbol];
            uint16_t huffman_distance_nbits = FIXED_DIST_NBITS[deflated_distance.symbol];

            bw_write_bits(
                bw,
                reverse_bits(huffman_distance_code, huffman_distance_nbits),
                huffman_distance_nbits
            );

            bw_write_bits(
                bw, 
                deflated_distance.extra_value,
                deflated_distance.extra_bits
            );
        }
    }
    uint16_t eob_code = FIXED_LITERAL_LENGTH_CODE[256];
    uint8_t eob_nbits = FIXED_LITERAL_LENGTH_NBITS[256];
    bw_write_bits(
        bw, 
        reverse_bits(eob_code, eob_nbits), 
        eob_nbits
    );
}


uint32_t crc32(const char *body, const size_t body_size){
    uint32_t crc = 0xFFFFFFFF;

    for (size_t i = 0; i < body_size; i++){
        crc = CRC32_TABLE[(crc ^ body[i]) & 0xFFu] ^ (crc >> 8);
    }

    return crc ^ 0xFFFFFFFF;
}

