#ifndef HTTP_COMPRESSION_H
#define HTTP_COMPRESSION_H

#include <sys/types.h>


void build_gzip_encoded_body(
    const char *body, size_t body_len,
    char **encoded_body, size_t* encoded_body_len
);


#endif

