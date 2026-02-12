#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "http_response.h"

size_t parse_target(const char* target, size_t target_len, slice_t *segs, size_t max_segs){
	if (target_len <= 1 || target[0] != '/') return 0;
	if (max_segs == 0) return (size_t) - 1;
	size_t segs_idx = 0;
	size_t cur_segs_size = 0;
	segs[segs_idx].p = &target[1];

	for (size_t i = 1; i < target_len; i++){
		if (target[i] == '/' && i != target_len - 1) {
			if (segs_idx + 1 >= max_segs) return (size_t) - 1;
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


int construct_response(
	char *method, size_t method_len,
	char *target, size_t target_len,
	char *version, size_t version_len,
	const char **response, size_t *response_len
){

	const char *ok_message = "HTTP/1.1 200 OK\r\n";
	size_t ok_len = strlen(ok_message);
	const char *not_found_message = "HTTP/1.1 404 Not Found\r\n";
	size_t not_found_len = strlen(not_found_message);

	size_t max_target_segs = 199;
	slice_t target_segs[max_target_segs];
	size_t target_size = parse_target(target, target_len, target_segs, max_target_segs);
	const char *content_type;
	size_t content_type_len;
	const char *content_length;	
	size_t content_length_len;
	char *body;
	size_t body_len;


	if (method_len == 3 && memcmp(method, "GET", 3) == 0 && target_len == 1 && memcmp(target, "/", 1) == 0){
		*response_len = ok_len + strlen("\r\n");
		*response = malloc(*response_len * sizeof(char));
		if (!*response) return 1;

		memcpy(*response, ok_message, ok_len);
		memcpy(*response + ok_len, "\r\n", 2);
		printf("Correct target and method.");
		return 0;
	}else if (target_size > 0 && target_segs[0].len == 4 && memcmp(target_segs[0].p, "echo", 4) == 0 && target_size >= 2){
		content_type = "Content-Type: text/plain\r\n";
		content_type_len = strlen(content_type);
		body = target_segs[1].p;
		body_len = target_segs[1].len;

		char content_length_hdr[64];
		int content_length_len = snprintf(content_length_hdr, sizeof(content_length_hdr), "Content-Length: %zu\r\n", body_len);

		*response_len = ok_len + content_type_len + content_length_len + 2 + body_len;
		*response = malloc(*response_len * sizeof(char));
		memcpy(*response, ok_message, ok_len);
		memcpy(*response + ok_len, content_type, content_type_len);
		memcpy(*response + ok_len + content_type_len, content_length_hdr, content_length_len);
		memcpy(*response + ok_len + content_type_len + content_length_len, "\r\n", 2);
		memcpy(*response + ok_len + content_type_len + content_length_len + 2, body, body_len);
		return 0;
	}else{
		*response_len = not_found_len + strlen("\r\n");
		*response = malloc(*response_len * sizeof(char));
		if (!*response) return 1;

		memcpy(*response, not_found_message, not_found_len);
		memcpy(*response + not_found_len, "\r\n", 2);
		printf("Target not recognised");
		return 0;
	}
}
