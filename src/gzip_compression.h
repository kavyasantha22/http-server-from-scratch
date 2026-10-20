#ifndef HTTP_COMPRESSION_H
#define HTTP_COMPRESSION_H

#include <sys/types.h>

typedef enum {
    TOKEN_LITERAL,
    TOKEN_MATCH
} lz_token_type_t;

typedef struct {
    lz_token_type_t type;
    union {
        struct {
            size_t length;
            size_t distance;
        } match;
        unsigned char literal;
    } data;
} lz_token_t;

void build_gzip_encoded_body(
    const char *body, size_t body_len,
    char **encoded_body, size_t* encoded_body_len
);

void lz_tokenisation(
    const char *body, size_t body_len,
    lz_token_t **tokenised_body, size_t *tokenised_body_len
);


#endif
