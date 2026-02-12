#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

/**
 * slice_t
 * A non-owning view into a byte sequence (typically a substring of an HTTP buffer).
 * - p   : pointer to first byte of the slice
 * - len : number of bytes in the slice
 *
 * NOTE: slice_t is NOT null-terminated. Use memcmp/len checks (not strcmp).
 */
typedef struct {
	const char *p;
	size_t len;
} slice_t;

/**
 * parse_target
 * Splits an HTTP request target (path) like "/echo/abc" into slash-separated segments
 * without copying. Writes slices into `segs` and returns the number of segments.
 *
 * Params:
 * - target, target_len : path buffer + length (not necessarily null-terminated)
 * - segs              : output array to receive slices
 * - max_segs          : capacity of `segs`
 *
 * Returns:
 * - 0          : invalid/too-short target OR does not start with '/'
 * - (size_t)-1 : error/sentinel indicating capacity issue (max_segs==0 or overflow)
 * - >0         : number of segments written to segs[0..n-1]
 *
 * Behavior notes:
 * - Trailing slash does not create an extra empty final segment.
 * - Repeated slashes in the middle can produce empty segments under current logic.
 */
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

/**
 * construct_response
 * Builds an HTTP/1.1 response message for the parsed request line.
 * On success, allocates a response buffer and writes:
 * - *response     : pointer to malloc'd response bytes
 * - *response_len : number of bytes in the response
 *
 * Caller is responsible for sending the response and freeing *response.
 *
 * Params:
 * - client_fd : client socket FD (currently unused in this implementation)
 * - method/target/version + *_len : slices into the request buffer
 * - response/response_len : output pointers for constructed response
 *
 * Returns:
 * - 0 on success
 * - non-zero on allocation/build failure
 *
 * Current routing intent:
 * - GET /         -> 200 OK, no headers/body (status-line + blank line)
 * - /echo/{str}   -> intended to echo string (currently incomplete)
 * - otherwise     -> 404 Not Found (status-line + blank line)
 *
 * IMPORTANT:
 * - Ensure every branch sets *response and *response_len.
 * - Do not use strcmp on non-null-terminated slices; prefer len+memcmp.
 */
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
 * parse_http_request
 * Splits a raw HTTP request buffer into:
 * - request line (up to first CRLF)
 * - headers block (from after first CRLF up to CRLFCRLF)
 * - body (everything after CRLFCRLF)
 *
 * Outputs are pointers into the original `buf` plus lengths.
 *
 * Params:
 * - buf, len : raw request bytes
 * - request_line/request_line_len : output slice for request line
 * - headers/headers_len           : output slice for header section
 * - body/body_len                 : output slice for body section
 *
 * Returns:
 * - 0 on success
 * - -1 if CRLF / CRLFCRLF delimiters are not found (malformed/incomplete request)
 */
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


/**
 * parse_request_line
 * Parses an HTTP request line of the form:
 *   METHOD SP TARGET SP VERSION
 * into three slices (pointers into buf + lengths).
 *
 * Params:
 * - buf, len : request-line bytes (no trailing CRLF)
 * - method/method_len   : output slice for HTTP method (e.g., "GET")
 * - target/target_len   : output slice for target path (e.g., "/echo/abc")
 * - version/version_len : output slice for version (e.g., "HTTP/1.1")
 *
 * Returns:
 * - 0 on success
 * - -1 if the request line doesn't contain the expected two spaces
 *
 * NOTE: Slices are not null-terminated; compare using len+memcmp.
 */
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
