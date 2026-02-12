#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include "http_parser.h"
#include "http_response.h"

/**
 * read_http_request
 * Reads raw bytes from client_fd into a newly malloc'd buffer (single recv call).
 * Null-terminates the buffer for convenience, but length is tracked separately.
 *
 * Params:
 * - client_fd : client socket FD
 * - out       : set to malloc'd buffer containing received bytes (plus '\0')
 * - out_len   : set to number of bytes received (excluding the added '\0')
 *
 * Returns:
 * - -1 on recv error (frees *out and sets outputs to safe values)
 * -  0 if the peer closed the connection (recv returns 0)
 * -  n (>0) number of bytes received otherwise
 *
 * NOTE: This reads at most 4095 bytes and does not loop; large/multi-packet
 * requests may require repeated reads to be fully robust.
 */
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

/**
 * main
 * Creates a TCP server on 0.0.0.0:4221, accepts a single client connection,
 * then loops:
 *  - read_http_request
 *  - parse_http_request
 *  - parse_request_line
 *  - construct_response
 *  - send() the built response
 *
 * Notes:
 * - Uses SO_REUSEADDR for convenience during frequent restarts.
 * - Current design allocates per request; caller should free buffers to avoid leaks.
 * - send() may send fewer bytes than requested; robust servers loop until all sent.
 */
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

	const char *response_message = NULL;
	size_t response_message_len = 0;

	while (1){
		client_fd = accept(server_fd, (struct sockaddr *) &client_addr, &client_addr_len);

		if (client_fd == -1){
			printf("Client connection failed: %s \n", strerror(errno));
			return 1;
		}
		printf("Client connected\n");
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

		construct_response(
			method, method_len, 
			target, target_len,
			version, version_len,
			headers, headers_len,
			&response_message, &response_message_len
		);

		int send_value = send(client_fd, response_message, response_message_len, 0);

		if (send_value == -1){
			printf("Failed to send welcome message: %s \n",strerror(errno));
			return 1;
		}
	}

	close(client_fd);
	printf("Client disconnected.\n");

	close(server_fd);
	printf("Server closed.\n");

	return 0;
}
