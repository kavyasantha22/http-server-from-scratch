#ifndef HTTP_RESPONSE_H
#define HTTP_RESPONSE_H
#include <stddef.h>
#include "http_parser.h"

typedef struct {
	const char *method;
	size_t method_len;
	const slice_t *target_segs;
	size_t target_segs_len;
	const slice_t *headers_segs;
	size_t headers_segs_len;
	const char* body;
	size_t body_len;
	const char* files_dir;
	char **response;
	size_t *response_len;
} http_request_t;

/* -----------------------------------------------------------------------
 * construct_response
 * -----------------------------------------------------------------------
 * Builds a complete, send-ready HTTP/1.1 response from parsed request parts.
 *
 * ASSUMES (inputs -- all slices come from the parser, see http_parser.h):
 *
 *   method / method_len:
 *     - e.g. "GET", "POST"1
 *     - No spaces, no CRLF.
 *
 *   target / target_len:
 *     - The raw path including leading '/'.  e.g. "/echo/abc"
 *     - No spaces, no CRLF.
 *     - This function calls parse_target() internally to split it
 *       into slash-separated segments.
 *
 *   version / version_len:
 *     - e.g. "HTTP/1.1"
 *     - No spaces, no CRLF. (currently unused in building the response)
 *
 *   headers / headers_len:
 *     - The raw headers block from parse_http_request.
 *     - Contains \r\n BETWEEN header lines, but NO leading/trailing \r\n.
 *     - This function calls parse_headers() internally to split it
 *       into individual header-line segments (each without \r\n).
 * 
 * 	 body / body_len:
 * 		- The raw body block from parse_http_request.
 *
 *   files_dir:
 *     - Null-terminated directory path for file serving (--directory flag).
 *     - May be NULL if --directory was not provided.
 *     - Expected to already include a trailing '/' if needed by the OS.
 * 
 *
 * PRODUCES (output):
 *
 *   response / response_len:
 *     - *response is a freshly malloc'd buffer containing the FULL
 *       HTTP response (status line + headers + blank line + body).
 *     - *response_len is the byte count of that buffer.
 *     - The response IS properly CRLF-terminated:
 *         "HTTP/1.1 200 OK\r\n"           <- status line
 *         "Content-Type: text/plain\r\n"   <- response headers (each has \r\n)
 *         "Content-Length: 5\r\n"
 *         "\r\n"                           <- blank line (end of headers)
 *         "hello"                          <- body (raw bytes, no trailing CRLF)
 *     - Caller MUST free(*response) after send().
 *
 * RETURNS:
 *    0  on success (response is ready to send)
 *    1  on allocation failure or internal error
 *    0  also when returning a 404 (the 404 response is still valid to send)
 *
 * ROUTING:
 *   GET  /                  -> 200 OK, empty body
 *   GET  /echo/{str}        -> 200 OK, text/plain, echoes {str}
 *   GET  /user-agent        -> 200 OK, text/plain, echoes User-Agent value
 *   GET  /files/{filename}  -> 200 OK, application/octet-stream, file contents
 *   POST /files/{filename}  -> 201 Created (WIP)
 *   anything else           -> 404 Not Found
 */
int construct_response(
	const char *method, size_t method_len,
	const char *target, size_t target_len,
	const char *version, size_t version_len,
    const char *headers, size_t headers_len,
	const char *body, size_t body_len,
	const char* files_dir, 
	char **response, size_t *response_len
);

#endif

