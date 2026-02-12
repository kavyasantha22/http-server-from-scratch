#ifndef HTTP_PARSER_H
#define HTTP_PARSER_H
#include <stddef.h>


/**
 * parse_http_request
 * Splits a raw HTTP request buffer into:
 * - request line (up to first CRLF)
 * - headers block (from after first CRLF up to CRLFCRLF)
 * - body (everything after CRLFCRLF)
 *
 * Outputs are pointers into the original `buf` plus lengths.
 *
 * Params:
 * - buf, len : raw request bytes
 * - request_line/request_line_len : output slice for request line
 * - headers/headers_len           : output slice for header section
 * - body/body_len                 : output slice for body section
 *
 * Returns:
 * - 0 on success
 * - -1 if CRLF / CRLFCRLF delimiters are not found (malformed/incomplete request)
 */
int parse_http_request(
	const char* buf, size_t len,
	const char** request_line, size_t *request_line_len,
	const char** headers, size_t *headers_len,
	const char **body, size_t *body_len
);


/**
 * parse_request_line
 * Parses an HTTP request line of the form:
 *   METHOD SP TARGET SP VERSION
 * into three slices (pointers into buf + lengths).
 *
 * Params:
 * - buf, len : request-line bytes (no trailing CRLF)
 * - method/method_len   : output slice for HTTP method (e.g., "GET")
 * - target/target_len   : output slice for target path (e.g., "/echo/abc")
 * - version/version_len : output slice for version (e.g., "HTTP/1.1")
 *
 * Returns:
 * - 0 on success
 * - -1 if the request line doesn't contain the expected two spaces
 *
 * NOTE: Slices are not null-terminated; compare using len+memcmp.
 */
int parse_request_line(
	const char* buf, size_t len,
	const char** method, size_t *method_len,
	const char** target, size_t *target_len,
	const char** version, size_t *version_len
);

#endif

