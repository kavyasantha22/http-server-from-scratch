#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "http_parser.h"


size_t get_content_length(const slice_t* headers_segs, size_t headers_segs_size){
	for (size_t i = 0; i < headers_segs_size; i++){
		if (headers_segs[i].len >= 16 && memcmp(headers_segs[i].p, "Content-Length: ", 16) == 0){
			char temp[headers_segs[i].len - 16];
			memcpy(temp, headers_segs[i].p + 16, headers_segs[i].len - 16);
			return atoi(temp);
		}
	}
	return -1;
}

/*
 * See http_parser.h for full documentation of every function's
 * input assumptions, output conventions, and CRLF/slash rules.
 */
int contains_header_end(const char *start, const char *end){
	int c = 0;
	while (start + c + 4 <= end){
		if (memcmp(start + c, "\r\n\r\n", 4) == 0) return c;
		c++;
	}
	return -1;
}


int parse_http_request(
	const char* buf, size_t len,
	const char** request_line, size_t *request_line_len,
	const char** headers, size_t *headers_len,
	const char** body, size_t *body_len
){
	/*
	 * Walk the buffer looking for two boundaries:
	 *   state 0 -> 1 : first \r\n       (end of request line)
	 *   state 1 -> 2 : \r\n\r\n         (end of headers / start of body)
	 *
	 * Output slices point INTO buf and exclude the delimiter bytes.
	 *
	 * Example buffer (dots = bytes we output, X = delimiter we skip):
	 *   GET / HTTP/1.1 \r\n Host: x \r\n\r\n  body...
	 *   [--request_line--]XX[headers]XXXX[body]
	 */
	size_t i = 0;
	int state = 0;
	*request_line = buf;
	size_t last_boundary = 0;

	while (i + 3 < len){
		if (state == 0 && buf[i] == '\r' && buf[i + 1] == '\n'){
			*request_line_len = i - last_boundary;
			*headers = buf + i + 2;
			last_boundary = i + 2;
			state++;
			i += 1;
		}else if (state == 1 && buf[i] == '\r' && buf[i + 1] == '\n' && buf[i + 2] == '\r' && buf[i + 3] == '\n') {
			*headers_len = i - last_boundary;
			*body = buf + i + 4;
			last_boundary = i + 4;
			state++;
			i += 3;
			break;
		}
		i++;
	}
	if (state != 2) return -1;
	*body_len = len - last_boundary;
	return 0;
}	


int parse_request_line(
	const char* buf, size_t len,
	const char** method, size_t *method_len,
	const char** target, size_t *target_len,
	const char** version, size_t *version_len
){
	/*
	 * Split on spaces:  "GET /echo/abc HTTP/1.1"
	 *                    ^^^·^^^^^^^^^·^^^^^^^^
	 *                   method target   version
	 *
	 * No CRLF in input or any output slice.
	 */
	int state = 0;
	*method = buf;
	int last_boundary = 0;
	for (size_t i = 0; i < len; i++){
		if (buf[i] == ' '){
			if (state == 0){
				*method_len = i - last_boundary;
				last_boundary = i + 1;
				*target = buf + i + 1;
			}else if (state == 1){
				*target_len = i - last_boundary;
				last_boundary = i + 1;
				*version = buf + i + 1;
				*version_len = len - last_boundary;
				return 0;
			}
			state++;
		}
	}
	return -1;
}


size_t parse_target(const char* target, size_t target_len, slice_t *segs, size_t max_segs){
	/*
	 * Input:  "/echo/abc"   (starts with '/', no CRLF, no spaces)
	 * Output: segs[0]="echo"  segs[1]="abc"
	 *
	 * The leading '/' is consumed. Segments do NOT contain '/'.
	 * Trailing '/' is ignored (no empty final segment).
	 */
	if (target_len <= 1 || target[0] != '/') return 0;
	if (max_segs == 0) return (size_t) -1;
	size_t segs_idx = 0;
	size_t cur_segs_size = 0;
	segs[segs_idx].p = &target[1];

	for (size_t i = 1; i < target_len; i++){
		if (target[i] == '/' && i != target_len - 1) {
			if (segs_idx + 1 >= max_segs) return (size_t) -1;
			segs[segs_idx].len = cur_segs_size;
			segs_idx++;
			segs[segs_idx].p = &target[i + 1];
			cur_segs_size = 0;
		}
		else cur_segs_size++;
	}
	segs[segs_idx].len = cur_segs_size;
	return segs_idx + 1;
}


size_t parse_headers(const char* headers, size_t headers_len, slice_t *segs, size_t max_segs){
	/*
	 * Input from parse_http_request (NO leading/trailing \r\n):
	 *   "Host: localhost\r\nUser-Agent: curl\r\nAccept: star-slash-star"
	 *
	 * Output segments -- each is one header line WITHOUT \r\n:
	 *   segs[0] = "Host: localhost"
	 *   segs[1] = "User-Agent: curl"
	 *   segs[2] = "Accept: star-slash-star"
	 *
	 * The \r\n between headers is consumed as a delimiter, not stored.
	 */
	if (headers_len == 0) return 0;
	if (max_segs == 0) return (size_t) -1;
	size_t segs_idx = 0;
	size_t cur_segs_size = 0;
	segs[segs_idx].p = &headers[0];

    size_t i = 0;
    while (i + 1 < headers_len){
        if (i + 2 == headers_len){
            segs[segs_idx].len = cur_segs_size + 2;
            break;
        } 
        if (headers[i] == '\r' && headers[i + 1] == '\n'){
            segs[segs_idx].len = cur_segs_size;
            segs_idx++;
            i += 2;
            if (segs_idx >= max_segs) return (size_t) -1;
            segs[segs_idx].p = &headers[i];
            cur_segs_size = 0;
        }else{
            cur_segs_size++;
            i++;
        }
    }
    return segs_idx + 1;
}


ssize_t parse_accept_encodings(
	const char *accepted_encodings,
	size_t accepted_encodings_size,
	slice_t *segs,
	size_t max_segs
){	
	if (accepted_encodings_size == 0) return 0;
	if (max_segs == 0) return -1;

	if (accepted_encodings_size > 17 && memcmp(accepted_encodings, "Accept-Encoding: ", 17) == 0){
		size_t segs_idx = 0;
		size_t cur_segs_size = 0;

		size_t i = 17;
		segs[segs_idx].p = &accepted_encodings[i];
		while (i < accepted_encodings_size){
			if (accepted_encodings[i] == ','){
            	segs[segs_idx].len = cur_segs_size;
				segs_idx++;

            	if (segs_idx >= max_segs) return -1;
				i += 2;
            	segs[segs_idx].p = &accepted_encodings[i];
				cur_segs_size = 0;
			}else{
				i++;
				cur_segs_size++;
			}
		}
		segs[segs_idx].len = cur_segs_size;
		return segs_idx + 1;
	}else return 0;
}