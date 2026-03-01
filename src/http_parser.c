#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "http_parser.h"


int parse_http_request(
	const char* buf, size_t len,
	const char** request_line, size_t *request_line_len,
	const char** headers, size_t *headers_len,
	const char **body, size_t *body_len
){
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
	if (state != 2) return - 1;
	*body_len = len - last_boundary;

	// char *temp = *request_line;
	// for (size_t i = 0; i < *request_line_len; i++) {
	// 	if (*(temp + i) == '\r') printf("\\r");
	// 	else if (*(temp + i) == '\n') printf("\\n\n"); // also break line visually
	// 	else putchar(*(temp + i));
	// }
	// printf("\n");
	// printf("\n");
	// temp = *headers;
	// for (size_t i = 0; i < *headers_len; i++) {
	// 	if (*(temp + i) == '\r') printf("\\r");
	// 	else if (*(temp + i) == '\n') printf("\\n\n"); // also break line visually
	// 	else putchar(*(temp + i));
	// }
	// printf("\n");
	// printf("\n");
	// temp = *body;
	// for (size_t i = 0; i < *body_len; i++) {
	// 	if (*(temp + i) == '\r') printf("\\r");
	// 	else if (*(temp + i) == '\n') printf("\\n\n"); // also break line visually
	// 	else putchar(*(temp + i));
	// }

	return 0;
}	


int parse_request_line(
	const char* buf, size_t len,
	const char** method, size_t *method_len,
	const char** target, size_t *target_len,
	const char** version, size_t *version_len
){
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
    