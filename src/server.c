#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include "http_parser.h"
#include "http_response.h"
#include "server.h"

static const char *g_files_dir = NULL;

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
	int header_end_found = -1;
	int bytes_read = 0;
	*out = malloc(MAX_HEADER_SIZE);

	while (bytes_read < MAX_HEADER_SIZE && header_end_found == -1){
		ssize_t cur_bytes_read = recv(client_fd, *out + bytes_read, MAX_HEADER_SIZE - bytes_read, 0);
		if (cur_bytes_read <= 0) return cur_bytes_read;

		bytes_read += cur_bytes_read;
		header_end_found = contains_header_end(*out, *out + bytes_read);
	}
	if (header_end_found == -1){
		printf("HEADER IS TOO BIG.");
		return -1;
	}

	size_t request_line_len;
	const char* unused_buf;
	size_t unused_sze;
	const char *headers;
	size_t headers_len;
	parse_http_request(*out, bytes_read, &unused_buf, &request_line_len, &headers, &headers_len, &unused_buf, &unused_sze);

	int body_bytes_read = bytes_read - (header_end_found + 4);

	size_t headers_segs_len = 1000;
	slice_t* headers_segs = malloc(headers_segs_len * sizeof(slice_t));
	if (!headers_segs) return -1;

	headers_segs_len = parse_headers(headers, headers_len, headers_segs, headers_segs_len);
	if (headers_segs_len == (size_t)-1) return -1;

	size_t body_len = get_content_length(headers_segs, headers_segs_len);
	free(headers_segs);
	if (body_len == (size_t)-1) body_len = 0;

	*out_len = (header_end_found + 4) + body_len;
	*out = realloc(*out, *out_len);
	if (!*out) return -1;


	while ((size_t)body_bytes_read < body_len){
		ssize_t cur_bytes_read = recv(client_fd, *out + bytes_read, body_len - body_bytes_read, 0);
		if (cur_bytes_read <= 0) return -1;

		bytes_read += cur_bytes_read;
		body_bytes_read += cur_bytes_read;
	}

	return bytes_read;
}


void *handle_client(void *arg){
	int client_fd = *(int *) arg;
	free(arg);
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

	char *response_message = NULL;
	size_t response_message_len = 0;

	printf("Client connected\n");

	while (1){
		int request_value = read_http_request(client_fd, &http_request, &http_request_len);

		if (request_value <= 0){
			break;
		} 

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

		int construct_response_val = construct_response(
			method, method_len, 
			target, target_len,
			version, version_len,
			headers, headers_len,
			body, body_len,
			g_files_dir,
			&response_message, &response_message_len
		);

		if (construct_response_val != 0) {
			printf("construct_response failed\n");
			break;
		}

		int send_value = send(client_fd, response_message, response_message_len, 0);

		if (send_value == -1){
			printf("Failed to send welcome message: %s \n",strerror(errno));
			break;
		}
		printf("SENT!\n");
		printf("%.*s", (int)response_message_len, response_message);

		free(http_request);
		http_request = NULL;
		http_request_len = 0;

		free(response_message);
		response_message = NULL;
		response_message_len = 0;

		// comment the break if you want sustained connection
		break;
	}
	free(http_request);
	http_request = NULL;
	http_request_len = 0;
	free(response_message);
	response_message = NULL;
	response_message_len = 0;
	close(client_fd);
	printf("Client disconnected.\n");
	return NULL;
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
int start_server(int argc, char **argv) {
	// Disable output buffering
	setbuf(stdout, NULL);
 	setbuf(stderr, NULL);

	// You can use print statements as follows for debugging, they'll be visible when running tests.
	printf("Logs from your program will appear here!\n");
	
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
	
	int connection_backlog = 128;
	if (listen(server_fd, connection_backlog) != 0) {
		printf("Listen failed: %s \n", strerror(errno));
		return 1;
	}

	// process direcoty argument if exist
	for (int i = 1; i < argc; i++){
		if (strcmp(argv[i], "--directory") == 0 && i + 1 < argc){
			g_files_dir = argv[i + 1];
			i++;
		}
	}

	if (g_files_dir){
		printf("files_dir set to: %s\n", g_files_dir);
	}
	
	printf("Waiting for a client to connect...\n");
	client_addr_len = sizeof(client_addr);

	int client_fd;

	while (1){
		client_fd = accept(server_fd, (struct sockaddr *) &client_addr, (socklen_t *)&client_addr_len);

		if (client_fd < 0){
			if (errno == EINTR) continue;
        	perror("accept");
        	continue;
		}

		int *cfd = malloc(sizeof(int));
		if (!cfd) {
			printf("failed to malloc client fd");
			continue;
		}
		*cfd = client_fd;

		pthread_t tid;
		if (pthread_create(&tid, NULL, handle_client, cfd) != 0){
			free(cfd);
			close(client_fd);
			printf("failed to create a seperate thread for the client");
			continue;
		}
		pthread_detach(tid);
	}

	close(server_fd);
	printf("Server closed.\n");

	return 0;
}
