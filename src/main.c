#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

int read_http_request(int client_fd, char **out, size_t *out_len){
	size_t cap = 4095;
	*out = malloc((cap + 1) * sizeof(char));
	if (!*out) return -1;

	ssize_t n = recv(client_fd, *out, cap, 0);
	if (n < 0){
		printf("Receiving message failed: %s", strerror(errno));
		free(*out);
		*out = NULL;
		*out_len = 0;
		return -1;
	}else if (n == 0){
		printf("Socket closed.");
		(*out)[0] = '\0';
		*out_len = 0;
		return 0;
	}
	(*out)[n] = '\0';
	*out_len = (size_t) n;
	return n;
}


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



int main() {
	// Disable output buffering
	setbuf(stdout, NULL);
 	setbuf(stderr, NULL);

	// You can use print statements as follows for debugging, they'll be visible when running tests.
	printf("Logs from your program will appear here!\n");

	// TODO: Uncomment the code below to pass the first stage
	
	// server_fd is a file descriptor for the server socket.
	// client_addr_len is the size of the client's addr
	int server_fd, client_addr_len;
	struct sockaddr_in client_addr;
	
	server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (server_fd == -1) {
		printf("Socket creation failed: %s...\n", strerror(errno));
		return 1;
	}
	
	// Since the tester restarts your program quite often, setting SO_REUSEADDR
	// ensures that we don't run into 'Address already in use' errors
	int reuse = 1;
	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
		printf("SO_REUSEADDR failed: %s \n", strerror(errno));
		return 1;
	}
	
	struct sockaddr_in serv_addr = { .sin_family = AF_INET ,
									 .sin_port = htons(4221),
									 .sin_addr = { htonl(INADDR_ANY) },
									};
	
	if (bind(server_fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) != 0) {
		printf("Bind failed: %s \n", strerror(errno));
		return 1;
	}
	
	int connection_backlog = 5;
	if (listen(server_fd, connection_backlog) != 0) {
		printf("Listen failed: %s \n", strerror(errno));
		return 1;
	}
	
	printf("Waiting for a client to connect...\n");
	client_addr_len = sizeof(client_addr);

	int client_fd;
	
	client_fd = accept(server_fd, (struct sockaddr *) &client_addr, &client_addr_len);

	if (client_fd == -1){
		printf("Client connection failed: %s \n", strerror(errno));
		return 1;
	}
	printf("Client connected\n");

	const char *ok_message = "HTTP/1.1 200 OK\r\n\r\n";
	const char *not_found_message = "HTTP/1.1 404 Not Found\r\n\r\n";

	// if (send(client_fd, ok_message, strlen(ok_message), 0) == -1){
	// 	printf("Failed to send welcome message: %s \n",strerror(errno));
	// 	return 1;
	// }

	char *http_request = NULL;
	size_t http_request_len = 0;

	const char *request_line = NULL;
	size_t request_line_len = 0;
	const char *headers = NULL;
	size_t headers_len = 0;
	const char *body = NULL;
	size_t body_len = 0;

	const char *method = NULL;
	size_t method_len = 0;
	const char *target = NULL;
	size_t target_len = 0;
	const char *version = NULL;
	size_t version_len = 0;

	while (1){
		read_http_request(client_fd, &http_request, &http_request_len);
		parse_http_request(
			http_request, http_request_len,
			&request_line, &request_line_len,
			&headers, &headers_len,
			&body, &body_len
		);

		parse_request_line(
			request_line, request_line_len,
			&method, &method_len,
			&target, &target_len,
			&version, &version_len
		);

		if (method_len == 3 && memcmp(method, "GET", 3) == 0 && target_len == 1 && memcmp(target, "/", 1) == 0){
			if (send(client_fd, ok_message, strlen(ok_message), 0) == -1){
				printf("Failed to send welcome message: %s \n",strerror(errno));
				return 1;
			}
			printf("Correct target and method.");
		}else{
			if (send(client_fd, not_found_message, strlen(not_found_message), 0) == -1){
				printf("Failed to send welcome message: %s \n",strerror(errno));
				return 1;
			}
			printf("NOT Correct target and method.");

		}
	}

	close(client_fd);
	printf("Client disconnected.\n");

	close(server_fd);
	printf("Server closed.\n");

	return 0;
}
