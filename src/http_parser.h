#ifndef HTTP_PARSER_H
#define HTTP_PARSER_H
#include <stddef.h>
#include <stdbool.h>

/*
 * ==========================================================================
 *  CONVENTIONS USED THROUGHOUT THIS MODULE
 * ==========================================================================
 *
 *  All outputs are NON-OWNING slices (pointer + length) into the original
 *  buffer. Nothing is copied; nothing is null-terminated.
 *  Always compare with memcmp + length, never strcmp.
 *
 *  "CRLF" means the two-byte sequence  \r\n  (0x0D 0x0A).
 *
 * ==========================================================================
 */

/**
 * slice_t -- a non-owning view into a byte buffer.
 *
 *   .p   = pointer to first byte
 *   .len = number of bytes
 *
 * NOT null-terminated. Always use memcmp/len, never strcmp.
 */
typedef struct {
	const char *p;
	size_t len;
} slice_t;


/* -----------------------------------------------------------------------
 * parse_http_request
 * -----------------------------------------------------------------------
 * Splits a raw HTTP request buffer into three regions.
 *
 * ASSUMES (input):
 *   - buf contains a complete HTTP request with at least one CRLF and
 *     one CRLFCRLF sequence.
 *   - Layout in the buffer:
 *
 *       REQUEST_LINE \r\n HEADER_1 \r\n HEADER_2 \r\n ... \r\n \r\n BODY
 *       ^                ^                                       ^
 *       |                |                                       |
 *       first byte    first CRLF                           double-CRLF
 *
 * PRODUCES (output) -- what each slice looks like:
 *
 *   request_line / request_line_len:
 *     - Points to the first byte of buf.
 *     - Length goes up to (but does NOT include) the first \r\n.
 *     - Contains: "GET /path HTTP/1.1"
 *     - No CRLF at start or end.
 *
 *   headers / headers_len:
 *     - Starts right AFTER the first \r\n (skips those 2 bytes).
 *     - Length goes up to (but does NOT include) the \r\n\r\n boundary.
 *     - Contains internal \r\n between individual header lines.
 *     - Does NOT have a leading or trailing \r\n.
 *     - Example (3 headers, vertical bars show boundaries):
 *         |Host: localhost\r\nUser-Agent: curl\r\nAccept: star-slash-star|
 *     - If there is only one header, there are no \r\n at all:
 *         |Host: localhost|
 *
 *   body / body_len:
 *     - Starts right AFTER the \r\n\r\n (skips those 4 bytes).
 *     - Length is whatever remains in the buffer.
 *     - May be empty (body_len == 0) for bodiless requests like GET.
 *     - Raw bytes; no CRLF framing added or removed.
 *
 * RETURNS:
 *    0  on success
 *   -1  if either the first CRLF or the CRLFCRLF was not found
 *       (malformed or incomplete request)
 */
int parse_http_request(
	const char* buf, size_t len,
	const char** request_line, size_t *request_line_len,
	const char** headers, size_t *headers_len,
	const char **body, size_t *body_len
);


/* -----------------------------------------------------------------------
 * parse_request_line
 * -----------------------------------------------------------------------
 * Splits an HTTP request line into its three space-separated parts.
 *
 * ASSUMES (input):
 *   - buf/len is the request_line slice from parse_http_request.
 *   - No CRLF -- it was already stripped.
 *   - Layout:  METHOD SP TARGET SP VERSION
 *     Example: "GET /echo/abc HTTP/1.1"
 *
 * PRODUCES (output) -- each slice:
 *
 *   method / method_len:
 *     - "GET", "POST", etc.
 *     - No spaces, no CRLF.
 *
 *   target / target_len:
 *     - The path exactly as it appears, including the leading '/'.
 *     - Examples: "/", "/echo/abc", "/files/test.txt"
 *     - No spaces, no CRLF.
 *
 *   version / version_len:
 *     - "HTTP/1.1" (or whatever the client sent).
 *     - No spaces, no CRLF.
 *
 * RETURNS:
 *    0  on success (found exactly two spaces)
 *   -1  if fewer than two spaces were found
 */
int parse_request_line(
	const char* buf, size_t len,
	const char** method, size_t *method_len,
	const char** target, size_t *target_len,
	const char** version, size_t *version_len
);


/* -----------------------------------------------------------------------
 * parse_target
 * -----------------------------------------------------------------------
 * Splits a URL target path into slash-separated segments.
 *
 * ASSUMES (input):
 *   - target/target_len is the target slice from parse_request_line.
 *   - Starts with '/' (rejects input otherwise).
 *   - No spaces, no CRLF.
 *   - Example: "/echo/abc"
 *
 * PRODUCES (output) -- each segs[i]:
 *
 *   - The text BETWEEN slashes. Leading '/' is consumed, not stored.
 *   - Segments do NOT contain any '/' characters.
 *   - No CRLF in any segment.
 *
 *   Examples:
 *     "/echo/abc"    -> segs[0]="echo"  segs[1]="abc"    returns 2
 *     "/files/a.txt" -> segs[0]="files" segs[1]="a.txt"  returns 2
 *     "/"            -> returns 0 (target_len <= 1)
 *     "/echo/"       -> segs[0]="echo"                   returns 1
 *                       (trailing slash is ignored)
 *     "/a//b"        -> segs[0]="a" segs[1]="" segs[2]="b"  returns 3
 *                       (double slash produces an empty segment)
 *
 * RETURNS:
 *    0          target too short (<= 1 byte) or doesn't start with '/'
 *    (size_t)-1 max_segs is 0, or too many segments to fit
 *    >0         number of segments written
 */
size_t parse_target(const char* target, size_t target_len, slice_t *segs, size_t max_segs);


/* -----------------------------------------------------------------------
 * parse_headers
 * -----------------------------------------------------------------------
 * Splits the headers block into individual header-line segments.
 *
 * ASSUMES (input):
 *   - headers/headers_len is the headers slice from parse_http_request.
 *   - Contains \r\n BETWEEN header lines, but:
 *       * NO leading \r\n  (first byte is start of first header)
 *       * NO trailing \r\n (last byte is end of last header value)
 *     This is because parse_http_request already stripped the boundary CRLFs.
 *   - Example input (pipes show start/end of the slice):
 *       |Host: localhost\r\nUser-Agent: curl\r\nAccept: star-slash-star|
 *
 * PRODUCES (output) -- each segs[i]:
 *
 *   - One complete header line, e.g. "Host: localhost"
 *   - Does NOT include the \r\n delimiter.
 *   - The full line including "Key: Value" is in one segment.
 *
 *   Example:
 *     Input: "Host: localhost\r\nUser-Agent: curl\r\nAccept: star-slash-star"
 *     segs[0] = "Host: localhost"      	(len=15)
 *     segs[1] = "User-Agent: curl"     	(len=16)
 *     segs[2] = "Accept: star-slash-star"  (len=11)
 *     returns 3
 *
 *   To extract a header value, skip past "Key: " using an offset.
 *   e.g. for "User-Agent: curl", value starts at p+12 with len-12.
 *
 * RETURNS:
 *    0          empty headers block (headers_len == 0)
 *    (size_t)-1 max_segs is 0, or too many headers to fit
 *    >0         number of header segments written
 */
size_t parse_headers(const char* headers, size_t headers_len, slice_t *segs, size_t max_segs);


/* -----------------------------------------------------------------------
 * contains_header_end
 * -----------------------------------------------------------------------
 * Scans a byte range for the \r\n\r\n sequence that marks the end of
 * HTTP headers.
 *
 * ASSUMES (input):
 *   - start points to the first byte to check.
 *   - end points one-past-the-last byte (half-open range [start, end)).
 *   - The range may be a subset of a larger buffer (e.g. only the
 *     bytes just received in the latest recv() call).
 *
 * RETURNS:
 *   true   if \r\n\r\n is found anywhere in [start, end)
 *   false  otherwise
 */
int contains_header_end(const char *start, const char *end);

size_t get_content_length(const slice_t* headers_segs, size_t headers_segs_size);

size_t parse_accept_encodings(
	const char *accepted_encodings,
	size_t accepted_encodings_size,
	slice_t *segs,
	size_t max_segs
);

#endif

