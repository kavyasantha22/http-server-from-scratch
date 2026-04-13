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

#define RESPONSE_HEADERS_SIZE 200

static int build_response(
	char **response, size_t *response_len,
	const char *status_line, size_t status_line_len,
	const char *response_headers, size_t response_headers_len,
	const char *response_body, size_t response_body_len
){
	*response_len = status_line_len + 2 + response_headers_len + 2 + response_body_len;
	*response = malloc(*response_len);
	if (*response == NULL) return 1;

	int offset = 0;

	memcpy(*response, status_line, status_line_len);
	offset += status_line_len;
	memcpy(*response + offset, "\r\n", 2);
	offset += 2;

	if (response_headers != NULL){
		memcpy(*response + offset, response_headers, response_headers_len);
		offset += response_headers_len;
	} 

	memcpy(*response + offset, "\r\n", 2);
	offset += 2;

	if (response_body != NULL){
		memcpy(*response + offset, response_body, response_body_len);
	}

	return 0;
}

static const slice_t *find_in_segs(
	const slice_t *segs, size_t segs_size,
	const char *target, size_t target_len
){
	for (size_t i = 0; i < segs_size; i++){
		if (segs[i].len >= target_len && memcmp(segs[i].p, target, target_len) == 0){
			return segs + i;
		}
	}
	return NULL;
}


static int handle_root(const http_request_t *request){
	if (request->method_len == 3 && memcmp(request->method, "GET", 3) == 0){
		const char *ok_status_line = "HTTP/1.1 200 OK";
		return build_response(
			request->response, request->response_len, 
			ok_status_line, strlen(ok_status_line),
			NULL, 0,
			NULL, 0
		);
	}else{
		const char *not_found_status_line = "HTTP/1.1 404 NOT FOUND";
		return build_response(
			request->response, request->response_len, 
			not_found_status_line, strlen(not_found_status_line),
			NULL, 0,
			NULL, 0
		);
	}
}


static int handle_echo(const http_request_t *request){
	if (request->method_len == 3 && memcmp(request->method, "GET", 3) == 0){
		char response_headers[RESPONSE_HEADERS_SIZE];
		int response_headers_len = 0;

		response_headers_len += snprintf(response_headers, RESPONSE_HEADERS_SIZE, "Content-Type: text/plain\r\n");

		const char *response_body = request->target_segs[1].p;
		const size_t response_body_len = request->target_segs[1].len;

		response_headers_len += snprintf(
			response_headers + response_headers_len, 
			RESPONSE_HEADERS_SIZE - response_headers_len, 
			"Content-Length: %zu\r\n", 
			response_body_len
		);

		const char *ok_status_line = "HTTP/1.1 200 OK";

		return build_response(
			request->response, request->response_len,
			ok_status_line, strlen(ok_status_line),
			response_headers, response_headers_len,
			response_body, response_body_len
		);
	}else{
		const char *not_found_status_line = "HTTP/1.1 404 NOT FOUND";
		return build_response(
			request->response, request->response_len, 
			not_found_status_line, strlen(not_found_status_line),
			NULL, 0,
			NULL, 0
		);
	}
}


static int handle_user_agent(const http_request_t *request){
	if (request->method_len == 3 && memcmp(request->method, "GET", 3) == 0){
		char response_headers[RESPONSE_HEADERS_SIZE];
		int response_headers_len = 0;

		response_headers_len += snprintf(response_headers, RESPONSE_HEADERS_SIZE, "Content-Type: application/octet-stream\r\n");

		char *user_agent_str = "User-Agent: ";
		const slice_t *user_agent_slice = find_in_segs(
			request->headers_segs, request->headers_segs_len,
			user_agent_str, strlen(user_agent_str)
		);

		if (user_agent_slice == NULL){
			return 1;
		}

		const char *response_body = user_agent_slice->p + strlen(user_agent_str);
        const size_t response_body_len = user_agent_slice->len - strlen(user_agent_str);

		response_headers_len += snprintf(
			response_headers + response_headers_len, 
			RESPONSE_HEADERS_SIZE - response_headers_len, 
			"Content-Length: %zu\r\n", 
			response_body_len
		);
		
		const char *ok_status_line = "HTTP/1.1 200 OK";

		return build_response(
			request->response, request->response_len,
			ok_status_line, strlen(ok_status_line),
			response_headers, response_headers_len,
			response_body, response_body_len
		);
	}else{
		const char *not_found_status_line = "HTTP/1.1 404 NOT FOUND";
		return build_response(
			request->response, request->response_len, 
			not_found_status_line, strlen(not_found_status_line),
			NULL, 0,
			NULL, 0
		);
	}
}

static int handle_files(const http_request_t *request){
	if (request->method_len == 3 && memcmp(request->method, "GET", 3) == 0){
		char response_headers[RESPONSE_HEADERS_SIZE];
		int response_headers_len = 0;

		response_headers_len += snprintf(response_headers, RESPONSE_HEADERS_SIZE, "Content-Type: application/octet-stream\r\n");

		const size_t files_name_len = request->target_segs[1].len;
		const char *files_name = request->target_segs[1].p;

		if (request->files_dir == NULL) return 1;

		size_t files_dir_len = strlen(request->files_dir);
		char* path = malloc(files_dir_len + files_name_len + 1);
		if (!path){
			return 1;
		}

		memcpy(path, request->files_dir, files_dir_len);
		memcpy(path + files_dir_len, files_name, files_name_len);
		path[files_dir_len + files_name_len] = '\0';

		int fd = open(path, O_RDONLY);
		if (fd < 0){
			free(path);
			const char *not_found_status_line = "HTTP/1.1 404 NOT FOUND";
			return build_response(
				request->response, request->response_len, 
				not_found_status_line, strlen(not_found_status_line),
				NULL, 0,
				NULL, 0
			);
		}
		free(path);

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

		size_t response_body_len = file_size;
		char *response_body = file_buf;

		response_headers_len += snprintf(
			response_headers + response_headers_len, 
			RESPONSE_HEADERS_SIZE - response_headers_len, 
			"Content-Length: %zu\r\n", 
			response_body_len
		);
		
		const char *ok_status_line = "HTTP/1.1 200 OK";

		return build_response(
			request->response, request->response_len,
			ok_status_line, strlen(ok_status_line),
			response_headers, response_headers_len,
			response_body, response_body_len
		);
		
	}else if (request->method_len == 4 && memcmp(request->method, "POST", 4) == 0){
		size_t files_dir_len = strlen(request->files_dir);

		char file_path[files_dir_len + request->target_segs[1].len + 1];
		memcpy(file_path, request->files_dir, files_dir_len); 
		memcpy(file_path + files_dir_len, request->target_segs[1].p, request->target_segs[1].len);

		file_path[files_dir_len + request->target_segs[1].len] = '\0';
		
		int write_fd = open(file_path, O_CREAT | O_WRONLY | O_TRUNC, 0644);
		if (write_fd < 0) {
			const char *internal_error_status_line = "HTTP/1.1 500 INTERNAL SEVER ERROR";
			return build_response(
				request->response, request->response_len, 
				internal_error_status_line, strlen(internal_error_status_line),
				NULL, 0,
				NULL, 0
			);
		}

		size_t bytes_written = write(write_fd, request->body, request->body_len);
		if (bytes_written != request->body_len){
			close(write_fd);
			const char *internal_error_status_line = "HTTP/1.1 500 INTERNAL SEVER ERROR";
			return build_response(
				request->response, request->response_len, 
				internal_error_status_line, strlen(internal_error_status_line),
				NULL, 0,
				NULL, 0
			);
		}
		close(write_fd);

		const char *created_status_line = "HTTP/1.1 201 CREATED";
		return build_response(
			request->response, request->response_len, 
			created_status_line, strlen(created_status_line),
			NULL, 0,
			NULL, 0
		);

	} else {
		const char *not_found_status_line = "HTTP/1.1 404 NOT FOUND";
		return build_response(
			request->response, request->response_len, 
			not_found_status_line, strlen(not_found_status_line),
			NULL, 0,
			NULL, 0
		);
	}
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
	size_t max_segs = 199;
	slice_t target_segs[max_segs];
	size_t target_size = parse_target(target, target_len, target_segs, max_segs);
    if (target_size == (size_t)-1) return 1;

    slice_t headers_segs[max_segs];
    size_t headers_segs_size = parse_headers(headers, headers_len, headers_segs, max_segs);
    if (headers_segs_size == (size_t)-1) return 1;

	const http_request_t request = {
		.method = method,
		.method_len = method_len,
		.target_segs = target_segs,
		.target_segs_len = target_size,
		.headers_segs = headers_segs,
		.headers_segs_len = headers_segs_size,
		.body = body,
		.body_len = body_len,
		.files_dir = files_dir,
		.response = response,
		.response_len = response_len
	};

	if (
		target_len == 1 && memcmp(target, "/", 1) == 0
	) return handle_root(&request);
	
	else if (
		target_size > 0 && target_segs[0].len == 4 && 
		memcmp(target_segs[0].p, "echo", 4) == 0 && target_size >= 2
	) return handle_echo(&request);
		
	else if (
		target_size > 0 && target_segs[0].len == 10 && 
		memcmp(target_segs[0].p, "user-agent", 10) == 0
	) return handle_user_agent(&request);

	else if (
		target_size >= 2 && target_segs[0].len == 5 && 
		memcmp(target_segs[0].p, "files", 5) == 0
	) return handle_files(&request);

	else {
		const char *not_found_status_line = "HTTP/1.1 404 NOT FOUND";
		return build_response(
			request.response, request.response_len, 
			not_found_status_line, strlen(not_found_status_line),
			NULL, 0,
			NULL, 0
		);
	}
}