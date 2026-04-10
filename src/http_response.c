#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include "http_response.h"
#include "http_parser.h"

static int construct_404_not_found(char **response, size_t *response_len) {
	const char *not_found_message = "HTTP/1.1 404 Not Found\r\n";
	size_t not_found_len = strlen(not_found_message);
    *response_len = not_found_len + 2;
    *response = malloc(*response_len);
    if (!*response) return 1;
    memcpy(*response, not_found_message, not_found_len);
    memcpy(*response + not_found_len, "\r\n", 2);
    return 0;
}


static int construct_500_internal_server_error(char **response, size_t *response_len){
	const char *internal_server_error_message = "HTTP/1.1 500 Internal Server Error\r\n";
	size_t internal_server_error_len = strlen(internal_server_error_message);
    *response_len = internal_server_error_len + 2;
    *response = malloc(*response_len);
    if (!*response) return 1;
    memcpy(*response, internal_server_error_message, internal_server_error_len);
    memcpy(*response + internal_server_error_len, "\r\n", 2);
    return 0;
}


static int construct_201_created(char **response, size_t *response_len){
	const char *created_message = "HTTP/1.1 201 Created\r\n";
	size_t created_len = strlen(created_message);
    *response_len = created_len + 2;
    *response = malloc(*response_len);
    if (!*response) return 1;
    memcpy(*response, created_message, created_len);
    memcpy(*response + created_len, "\r\n", 2);
    return 0;
}

int construct_response(
	const char *method, size_t method_len,
	const char *target, size_t target_len,
	const char *version, size_t version_len,
    const char *headers, size_t headers_len,
	const char* body, size_t body_len,
	const char* files_dir, 
	char **response, size_t *response_len
){
	// printf("FUCK OFF\n");
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
	char *response_body;
	size_t response_body_len;

	// printf("FUCK OFF\n");

	if (method_len == 3 && memcmp(method, "GET", 3) == 0 && target_len == 1 && memcmp(target, "/", 1) == 0){
		*response_len = ok_len + strlen("\r\n");
		*response = malloc(*response_len * sizeof(char));
		if (!*response) return 1;

		memcpy(*response, ok_message, ok_len);
		memcpy(*response + ok_len, "\r\n", 2);
		// printf("Correct target and method.");
		return 0;
	}else if (memcmp(method, "GET", 3) == 0 && target_size > 0 && target_segs[0].len == 4 && memcmp(target_segs[0].p, "echo", 4) == 0 && target_size >= 2){
		content_type = "Content-Type: text/plain\r\n";
		content_type_len = strlen(content_type);
		response_body = target_segs[1].p;
		response_body_len = target_segs[1].len;

		char content_length_hdr[64];
		int content_length_len = snprintf(content_length_hdr, sizeof(content_length_hdr), "Content-Length: %zu\r\n", response_body_len);

		*response_len = ok_len + content_type_len + content_length_len + 2 + response_body_len;
		*response = malloc(*response_len * sizeof(char));
		if (!*response) return 1;

		memcpy(*response, ok_message, ok_len);
		memcpy(*response + ok_len, content_type, content_type_len);
		memcpy(*response + ok_len + content_type_len, content_length_hdr, content_length_len);
		memcpy(*response + ok_len + content_type_len + content_length_len, "\r\n", 2);
		memcpy(*response + ok_len + content_type_len + content_length_len + 2, response_body, response_body_len);
		return 0;
	}else if (memcmp(method, "GET", 3) == 0 && target_size > 0 && target_segs[0].len == 10 && memcmp(target_segs[0].p, "user-agent", 10) == 0){
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
        response_body = headers_segs[user_agent_idx].p + 12;
        response_body_len = headers_segs[user_agent_idx].len - 12;
        char content_length_hdr[64];
		int content_length_len = snprintf(content_length_hdr, sizeof(content_length_hdr), "Content-Length: %zu\r\n", response_body_len);

        *response_len = ok_len + content_type_len + content_length_len + 2 + response_body_len;
		*response = malloc(*response_len * sizeof(char));
		if (!*response) return 1;
        // printf("%zu\n", *response_len);

		memcpy(*response, ok_message, ok_len);
		memcpy(*response + ok_len, content_type, content_type_len);
		memcpy(*response + ok_len + content_type_len, content_length_hdr, content_length_len);
		memcpy(*response + ok_len + content_type_len + content_length_len, "\r\n", 2);
		memcpy(*response + ok_len + content_type_len + content_length_len + 2, response_body, response_body_len);
        return 0;
    } else if (memcmp(method, "GET", 3) == 0 && target_size >= 2 && target_segs[0].len == 5 && memcmp(target_segs[0].p, "files", 5) == 0){
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
	}else if (memcmp(method, "POST", 4) == 0 && target_size >= 2 && target_segs[0].len == 5 && memcmp(target_segs[0].p, "files", 5) == 0) {
		size_t files_dir_len = strlen(files_dir);

		char file_name[files_dir_len + target_segs[1].len + 1];
		memcpy(file_name, files_dir, files_dir_len); 
		memcpy(file_name + files_dir_len, target_segs[1].p, target_segs[1].len);

		file_name[files_dir_len + target_segs[1].len] = '\0';
		
		int write_fd = open(file_name, O_CREAT | O_WRONLY | O_TRUNC, 0644);
		if (write_fd < 0) {
			construct_500_internal_server_error(response, response_len);
			return 1;
		}

		int bytes_written = write(write_fd, body, body_len);
		if (bytes_written != body_len){
			close(write_fd);
			construct_500_internal_server_error(response, response_len);
			return 1;
		}
		close(write_fd);

		return construct_201_created(response, response_len);
	} else return construct_404_not_found(response, response_len);
}


int compress_body(
	char* response_body, 
	size_t response_body_len, 
	slice_t
){
	
}
