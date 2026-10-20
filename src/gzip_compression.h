#ifndef GZIP_COMPRESSION_H
#define GZIP_COMPRESSION_H

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

/* Both functions return 0 on success or -1 on failure.
 * Successful output buffers belong to the caller and must be freed.
 * On failure, the output pointer is NULL and its length is zero. */
int build_gzip_encoded_body(
    const char *body, size_t body_len,
    char **encoded_body, size_t* encoded_body_len
);

int lz_tokenisation(
    const char *body, size_t body_len,
    lz_token_t **tokenised_body, size_t *tokenised_body_len
);


#endif
