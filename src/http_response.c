#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include "http_response.h"

size_t parse_target(const char* target, size_t target_len, slice_t *segs, size_t max_segs){
	if (target_len <= 1 || target[0] != '/') return 0;
	if (max_segs == 0) return (size_t) -1;
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

size_t parse_headers(const char* headers, size_t headers_len, slice_t *segs, size_t max_segs){
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

static int construct_404_not_found(const char **response, size_t *response_len) {
	const char *not_found_message = "HTTP/1.1 404 Not Found\r\n";
	size_t not_found_len = strlen(not_found_message);
    *response_len = not_found_len + 2;
    *response = malloc(*response_len);
    if (!*response) return 1;
    memcpy(*response, not_found_message, not_found_len);
    memcpy(*response + not_found_len, "\r\n", 2);
    return 0;
}

int construct_response(
	char *method, size_t method_len,
	char *target, size_t target_len,
	char *version, size_t version_len,
    char *headers, size_t headers_len,
	const char* files_dir, 
	const char **response, size_t *response_len
){
	const char *ok_message = "HTTP/1.1 200 OK\r\n";
	size_t ok_len = strlen(ok_message);
	size_t max_segs = 199;

	slice_t target_segs[max_segs];
	size_t target_size = parse_target(target, target_len, target_segs, max_segs);
    if (target_size == (size_t)-1) return 1;

    // printf("%zu\n", headers_len);
    slice_t headers_segs[max_segs];
    size_t headers_segs_size = parse_headers(headers, headers_len, headers_segs, max_segs);
    if (headers_segs_size == (size_t)-1) return 1;
    // printf("%zu\n", headers_segs_size);

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
		// printf("Correct target and method.");
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
		if (!*response) return 1;

		memcpy(*response, ok_message, ok_len);
		memcpy(*response + ok_len, content_type, content_type_len);
		memcpy(*response + ok_len + content_type_len, content_length_hdr, content_length_len);
		memcpy(*response + ok_len + content_type_len + content_length_len, "\r\n", 2);
		memcpy(*response + ok_len + content_type_len + content_length_len + 2, body, body_len);
		return 0;
	}else if (target_size > 0 && target_segs[0].len == 10 && memcmp(target_segs[0].p, "user-agent", 10) == 0){
		content_type = "Content-Type: text/plain\r\n";
		content_type_len = strlen(content_type);

        size_t user_agent_idx = (size_t)-1;
        for (size_t i = 0; i < headers_segs_size; i++){
            if (headers_segs[i].len >= 12 && memcmp(headers_segs[i].p, "User-Agent: ", 12) == 0){
                user_agent_idx = i;
                break;
            }
        }
        // printf("%zu\n", user_agent_idx);
        if (user_agent_idx == (size_t)-1) return 1;
        body = headers_segs[user_agent_idx].p + 12;
        body_len = headers_segs[user_agent_idx].len - 12;
        char content_length_hdr[64];
		int content_length_len = snprintf(content_length_hdr, sizeof(content_length_hdr), "Content-Length: %zu\r\n", body_len);

        *response_len = ok_len + content_type_len + content_length_len + 2 + body_len;
		*response = malloc(*response_len * sizeof(char));
		if (!*response) return 1;
        // printf("%zu\n", *response_len);

		memcpy(*response, ok_message, ok_len);
		memcpy(*response + ok_len, content_type, content_type_len);
		memcpy(*response + ok_len + content_type_len, content_length_hdr, content_length_len);
		memcpy(*response + ok_len + content_type_len + content_length_len, "\r\n", 2);
		memcpy(*response + ok_len + content_type_len + content_length_len + 2, body, body_len);
        return 0;
    } else if (target_size >= 2 && target_segs[0].len == 5 && memcmp(target_segs[0].p, "files", 5) == 0){
		content_type = "Content-Type: application/octet-stream\r\n";
		content_type_len = strlen(content_type);

		size_t files_name_len = target_segs[1].len;
		char *files_name = malloc(files_name_len + 1);
		if (!files_name) {
			return 1;
		}

		memcpy(files_name, target_segs[1].p, files_name_len);
		files_name[files_name_len] = '\0';

		size_t files_dir_len = strlen(files_dir);
		char* path = malloc(files_dir_len + files_name_len + 1);
		if (!path){
			free(files_name);
			return 1;
		}

		memcpy(path, files_dir, files_dir_len);
		memcpy(path + files_dir_len, files_name, files_name_len);
		path[files_dir_len + files_name_len] = '\0';

		int fd = open(path, O_RDONLY);
		if (fd < 0){
			free(path);
			free(files_name);
			return construct_404_not_found(response, response_len);
		}
		free(path);
		free(files_name);

		struct stat st;
		if (fstat(fd, &st) == -1){
			close(fd);
			return 1;
		}
		size_t file_size = (size_t) st.st_size;
		char *file_buf = malloc(file_size);
		if (!file_buf){
			close(fd);
			return 1;
		}
		size_t offset = 0;
		while (offset < file_size){
			// syntax is file descriptor, read to where, and how many bytes
			ssize_t n = read(fd, file_buf + offset, file_size - offset);
			if (n <= 0){
				free(file_buf);
				close(fd);
				return 1;
			}
			offset += (size_t)n;
		}
		close(fd);

		// build headers
		char content_length_hdr[64];
		int content_length_len = snprintf(content_length_hdr, sizeof(content_length_hdr), "Content-Length: %zu\r\n", file_size);

		// construct full response
		*response_len = ok_len + content_type_len + content_length_len + 2 + file_size;
		*response = malloc(*response_len);
		if (!*response) {
			free(file_buf);
			return 1;
		}
		size_t pos = 0;
		memcpy(*response + pos, ok_message, ok_len); 
		pos += ok_len;
		memcpy(*response + pos, content_type, content_type_len); 
		pos += content_type_len;
		memcpy(*response + pos, content_length_hdr, content_length_len);
		pos += content_length_len;
		memcpy(*response + pos, "\r\n", 2);
		pos += 2;
		memcpy(*response + pos, file_buf, file_size);
		pos += file_size;
		free(file_buf);
		return 0;

	}else return construct_404_not_found(response, response_len);
}
