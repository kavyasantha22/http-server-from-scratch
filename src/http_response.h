#ifndef HTTP_RESPONSE_H
#define HTTP_RESPONSE_H
#include <stddef.h>

/**
 * slice_t
 * A non-owning view into a byte sequence (typically a substring of an HTTP buffer).
 * - p   : pointer to first byte of the slice
 * - len : number of bytes in the slice
 *
 * NOTE: slice_t is NOT null-terminated. Use memcmp/len checks (not strcmp).
 */
typedef struct {
	const char *p;
	size_t len;
} slice_t;


/**
 * parse_target
 * Splits an HTTP request target (path) like "/echo/abc" into slash-separated segments
 * without copying. Writes slices into `segs` and returns the number of segments.
 *
 * Params:
 * - target, target_len : path buffer + length (not necessarily null-terminated)
 * - segs              : output array to receive slices
 * - max_segs          : capacity of `segs`
 *
 * Returns:
 * - 0          : invalid/too-short target OR does not start with '/'
 * - (size_t)-1 : error/sentinel indicating capacity issue (max_segs==0 or overflow)
 * - >0         : number of segments written to segs[0..n-1]
 *
 * Behavior notes:
 * - Trailing slash does not create an extra empty final segment.
 * - Repeated slashes in the middle can produce empty segments under current logic.
 */
size_t parse_target(const char* target, size_t target_len, slice_t *segs, size_t max_segs);


/**
 * construct_response
 * Builds an HTTP/1.1 response message for the parsed request line.
 * On success, allocates a response buffer and writes:
 * - *response     : pointer to malloc'd response bytes
 * - *response_len : number of bytes in the response
 *
 * Caller is responsible for sending the response and freeing *response.
 *
 * Params:
 * - client_fd : client socket FD (currently unused in this implementation)
 * - method/target/version + *_len : slices into the request buffer
 * - response/response_len : output pointers for constructed response
 *
 * Returns:
 * - 0 on success
 * - non-zero on allocation/build failure
 *
 * Current routing intent:
 * - GET /         -> 200 OK, no headers/body (status-line + blank line)
 * - /echo/{str}   -> intended to echo string (currently incomplete)
 * - otherwise     -> 404 Not Found (status-line + blank line)
 *
 * IMPORTANT:
 * - Ensure every branch sets *response and *response_len.
 * - Do not use strcmp on non-null-terminated slices; prefer len+memcmp.
 */
int construct_response(
	char *method, size_t method_len,
	char *target, size_t target_len,
	char *version, size_t version_len,
    char *headers, size_t headers_len,
	const char **response, size_t *response_len
);

#endif

