#ifndef SERVER_H
#define SERVER_H
#include <stddef.h>
#include <stdbool.h>

#define MAX_HEADER_SIZE (8192)



/* -----------------------------------------------------------------------
 * read_http_request
 * -----------------------------------------------------------------------
 * Reads a complete HTTP request from a connected socket into a
 * malloc'd buffer.
 *
 * ASSUMES (input):
 *   - client_fd is a valid, connected TCP socket file descriptor.
 *
 * PRODUCES (output):
 *
 *   *out:
 *     - Points to a malloc'd buffer containing the raw request bytes.
 *     - Null-terminated for convenience, but use out_len for the
 *       actual byte count (binary bodies may contain '\0').
 *     - Caller MUST free(*out) when done.
 *     - On error/close, *out is set to NULL.
 *
 *   *out_len:
 *     - Number of bytes received (excludes the trailing '\0').
 *     - Set to 0 on error or connection close.
 *
 * RETURNS:
 *   -1   recv() error (errno is set; *out freed and set to NULL)
 *    0   peer closed the connection gracefully
 *   >0   number of bytes received (same value stored in *out_len)
 *
 * NOTE: Currently reads at most MAX_HEADER_SIZE bytes for headers,
 * then a single recv for the body. Large/chunked requests may need
 * multiple reads for full robustness.
 */
int read_http_request(int client_fd, char **out, size_t *out_len);


/* -----------------------------------------------------------------------
 * handle_client
 * -----------------------------------------------------------------------
 * Thread entry point -- handles the full lifecycle of one client
 * connection.
 *
 * ASSUMES (input):
 *   - arg is a heap-allocated (int *) holding the client socket fd.
 *   - This function takes ownership of arg and frees it immediately.
 *
 * BEHAVIOUR:
 *   1. Reads one HTTP request   (read_http_request)
 *   2. Parses it                (parse_http_request -> parse_request_line)
 *   3. Builds a response        (construct_response)
 *   4. Sends the response       (send)
 *   5. Closes the client socket
 *
 *   All malloc'd buffers (request, response) are freed before return.
 *
 * RETURNS:
 *   Always NULL (required by pthread signature).
 */
void *handle_client(void *arg);

int server_start(int argc, char **argv);


#endif
