#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>
#include "../../src/server.h"
#include "../../src/http_parser.h"
#include "../../src/http_response.h"

/*
 * Helper: create a socketpair, write `req` (req_len bytes) to sv[0],
 * call read_http_request on sv[1], return the results.
 * Caller must free(*out) and close both sockets.
 */
static void send_and_read(const char *req, size_t req_len,
                          int sv[2], char **out, size_t *out_len, int *ret)
{
    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);
    ssize_t w = write(sv[0], req, req_len);
    assert(w == (ssize_t)req_len);
    *ret = read_http_request(sv[1], out, out_len);
}

static void cleanup(int sv[2], char *out)
{
    free(out);
    close(sv[0]);
    close(sv[1]);
}

/* -----------------------------------------------------------------
 * Test 1: POST with a body
 *
 * Verifies:
 *   - return value == total bytes
 *   - out_len == total bytes
 *   - full buffer matches the original request byte-for-byte
 *   - after parsing, body is exactly "12345"
 * ----------------------------------------------------------------- */
static void test_post_with_body(void)
{
    const char req[] =
        "POST /files/x HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Length: 5\r\n"
        "\r\n"
        "12345";
    size_t req_len = sizeof(req) - 1;

    int sv[2];
    char *out;
    size_t out_len;
    int ret;
    send_and_read(req, req_len, sv, &out, &out_len, &ret);

    assert(ret == (int)req_len);
    assert(out_len == req_len);
    assert(memcmp(out, req, req_len) == 0);

    const char *request_line, *headers, *body;
    size_t request_line_len, headers_len, body_len;
    assert(parse_http_request(out, out_len,
        &request_line, &request_line_len,
        &headers, &headers_len,
        &body, &body_len) == 0);

    assert(body_len == 5);
    assert(memcmp(body, "12345", 5) == 0);

    cleanup(sv, out);
    printf("PASS: test_post_with_body\n");
}

/* -----------------------------------------------------------------
 * Test 2: GET with no body (Content-Length: 0)
 *
 * Verifies:
 *   - works when body is empty
 *   - out_len covers headers + CRLFCRLF but no body bytes
 *   - parsed body_len == 0
 * ----------------------------------------------------------------- */
static void test_get_no_body(void)
{
    const char req[] =
        "GET / HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Length: 0\r\n"
        "\r\n";
    size_t req_len = sizeof(req) - 1;

    int sv[2];
    char *out;
    size_t out_len;
    int ret;
    send_and_read(req, req_len, sv, &out, &out_len, &ret);

    assert(ret > 0);
    assert(out_len == req_len);
    assert(memcmp(out, req, req_len) == 0);

    const char *request_line, *headers, *body;
    size_t request_line_len, headers_len, body_len;
    assert(parse_http_request(out, out_len,
        &request_line, &request_line_len,
        &headers, &headers_len,
        &body, &body_len) == 0);

    assert(body_len == 0);

    cleanup(sv, out);
    printf("PASS: test_get_no_body\n");
}

/* -----------------------------------------------------------------
 * Test 3: Multiple headers + body
 *
 * Verifies:
 *   - all headers are preserved and parseable
 *   - body content matches exactly
 *   - request line is correct
 * ----------------------------------------------------------------- */
static void test_multiple_headers_with_body(void)
{
    const char req[] =
        "POST /upload HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "User-Agent: testbot/1.0\r\n"
        "Accept: */*\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: 11\r\n"
        "\r\n"
        "hello world";
    size_t req_len = sizeof(req) - 1;

    int sv[2];
    char *out;
    size_t out_len;
    int ret;
    send_and_read(req, req_len, sv, &out, &out_len, &ret);

    assert(ret == (int)req_len);
    assert(out_len == req_len);

    const char *request_line, *headers, *body;
    size_t request_line_len, headers_len, body_len;
    assert(parse_http_request(out, out_len,
        &request_line, &request_line_len,
        &headers, &headers_len,
        &body, &body_len) == 0);

    assert(request_line_len == 21);
    assert(memcmp(request_line, "POST /upload HTTP/1.1", 21) == 0);

    assert(body_len == 11);
    assert(memcmp(body, "hello world", 11) == 0);

    slice_t hdr_segs[10];
    size_t hdr_count = parse_headers(headers, headers_len, hdr_segs, 10);
    assert(hdr_count == 5);

    assert(hdr_segs[0].len == 17 && memcmp(hdr_segs[0].p, "Host: example.com", 17) == 0);
    assert(hdr_segs[1].len == 23 && memcmp(hdr_segs[1].p, "User-Agent: testbot/1.0", 23) == 0);
    assert(hdr_segs[2].len == 11 && memcmp(hdr_segs[2].p, "Accept: */*", 11) == 0);
    assert(hdr_segs[3].len == 24 && memcmp(hdr_segs[3].p, "Content-Type: text/plain", 24) == 0);
    assert(hdr_segs[4].len == 18 && memcmp(hdr_segs[4].p, "Content-Length: 11", 18) == 0);

    cleanup(sv, out);
    printf("PASS: test_multiple_headers_with_body\n");
}

/* -----------------------------------------------------------------
 * Test 4: Large body (bigger than what fits in first recv)
 *
 * Verifies:
 *   - the body-reading loop correctly assembles a large body
 *   - out_len and body_len reflect the full size
 * ----------------------------------------------------------------- */
static void test_large_body(void)
{
    const size_t body_size = 4000;
    char body_data[body_size];
    memset(body_data, 'A', body_size);

    char length_str[32];
    int length_hdr_len = snprintf(length_str, sizeof(length_str),
        "Content-Length: %zu\r\n", body_size);

    const char *prefix =
        "POST /data HTTP/1.1\r\n"
        "Host: localhost\r\n";
    const char *separator = "\r\n";

    size_t req_len = strlen(prefix) + length_hdr_len + strlen(separator) + body_size;
    char *req = malloc(req_len);
    assert(req);

    size_t pos = 0;
    memcpy(req + pos, prefix, strlen(prefix));       pos += strlen(prefix);
    memcpy(req + pos, length_str, length_hdr_len);   pos += length_hdr_len;
    memcpy(req + pos, separator, strlen(separator));  pos += strlen(separator);
    memcpy(req + pos, body_data, body_size);          pos += body_size;
    assert(pos == req_len);

    int sv[2];
    char *out;
    size_t out_len;
    int ret;
    send_and_read(req, req_len, sv, &out, &out_len, &ret);

    assert(ret == (int)req_len);
    assert(out_len == req_len);

    const char *request_line, *headers, *body;
    size_t request_line_len, headers_len, body_len;
    assert(parse_http_request(out, out_len,
        &request_line, &request_line_len,
        &headers, &headers_len,
        &body, &body_len) == 0);

    assert(body_len == body_size);
    assert(memcmp(body, body_data, body_size) == 0);

    free(req);
    cleanup(sv, out);
    printf("PASS: test_large_body\n");
}

/* -----------------------------------------------------------------
 * Test 5: Single header, no body
 *
 * Verifies:
 *   - minimal valid request works
 *   - only one header segment is produced
 *   - body_len == 0
 * ----------------------------------------------------------------- */
static void test_single_header_no_body(void)
{
    const char req[] =
        "GET /index HTTP/1.1\r\n"
        "Content-Length: 0\r\n"
        "\r\n";
    size_t req_len = sizeof(req) - 1;

    int sv[2];
    char *out;
    size_t out_len;
    int ret;
    send_and_read(req, req_len, sv, &out, &out_len, &ret);

    assert(ret > 0);
    assert(out_len == req_len);

    const char *request_line, *headers, *body;
    size_t request_line_len, headers_len, body_len;
    assert(parse_http_request(out, out_len,
        &request_line, &request_line_len,
        &headers, &headers_len,
        &body, &body_len) == 0);

    assert(request_line_len == 19);
    assert(memcmp(request_line, "GET /index HTTP/1.1", 19) == 0);

    assert(body_len == 0);

    slice_t hdr_segs[10];
    size_t hdr_count = parse_headers(headers, headers_len, hdr_segs, 10);
    assert(hdr_count == 1);
    assert(hdr_segs[0].len == 17 && memcmp(hdr_segs[0].p, "Content-Length: 0", 17) == 0);

    cleanup(sv, out);
    printf("PASS: test_single_header_no_body\n");
}

/* -----------------------------------------------------------------
 * Test 6: Binary body (contains null bytes)
 *
 * Verifies:
 *   - null bytes in the body don't truncate the read
 *   - body_len and content match byte-for-byte
 * ----------------------------------------------------------------- */
static void test_binary_body(void)
{
    const char body_data[] = "\x00\x01\x02\x00\xff\xfe";
    size_t body_size = sizeof(body_data) - 1;

    char req[512];
    int hdr_len = snprintf(req, sizeof(req),
        "POST /bin HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Length: %zu\r\n"
        "\r\n", body_size);
    memcpy(req + hdr_len, body_data, body_size);
    size_t req_len = hdr_len + body_size;

    int sv[2];
    char *out;
    size_t out_len;
    int ret;
    send_and_read(req, req_len, sv, &out, &out_len, &ret);

    assert(ret == (int)req_len);
    assert(out_len == req_len);

    const char *request_line, *headers, *body;
    size_t request_line_len, headers_len, body_len;
    assert(parse_http_request(out, out_len,
        &request_line, &request_line_len,
        &headers, &headers_len,
        &body, &body_len) == 0);

    assert(body_len == body_size);
    assert(memcmp(body, body_data, body_size) == 0);

    cleanup(sv, out);
    printf("PASS: test_binary_body\n");
}

static void test_user_agent_without_content_length(void)
{
    const char req[] =
        "GET /user-agent HTTP/1.1\r\n"
        "Host: localhost:4221\r\n"
        "Accept: */*\r\n"
        "User-Agent: manual-test\r\n\r\n";
    int sv[2], ret;
    char *out;
    size_t out_len;
    send_and_read(req, sizeof(req) - 1, sv, &out, &out_len, &ret);
    assert(ret == sizeof(req) - 1);
    assert(out_len == sizeof(req) - 1);
    assert(memcmp(out, req, out_len) == 0);

    const char *line, *headers, *body;
    size_t line_len, headers_len, body_len;
    assert(parse_http_request(out, out_len, &line, &line_len,
        &headers, &headers_len, &body, &body_len) == 0);
    assert(body_len == 0);
    char *response;
    size_t response_len;
    assert(construct_response("GET", 3, "/user-agent", 11, "HTTP/1.1", 8,
        headers, headers_len, body, body_len, NULL,
        &response, &response_len) == 0);
    const char expected[] =
        "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n"
        "Content-Length: 11\r\n\r\nmanual-test";
    assert(response_len == sizeof(expected) - 1);
    assert(memcmp(response, expected, response_len) == 0);
    free(response);
    cleanup(sv, out);
    printf("PASS: test_user_agent_without_content_length\n");
}

static void test_read_failure_cleanup(void)
{
    const char *requests[] = {
        "",
        "GET / HTTP/1.1\r\nHost: localhost",
        "POST / HTTP/1.1\r\nContent-Length: 5\r\n\r\nab",
        "invalid\r\n\r\n"
    };
    for (size_t i = 0; i < sizeof(requests) / sizeof(requests[0]); i++){
        int sv[2];
        assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);
        size_t len = strlen(requests[i]);
        if (len) assert(write(sv[0], requests[i], len) == (ssize_t)len);
        assert(shutdown(sv[0], SHUT_WR) == 0);
        char *out = NULL;
        size_t out_len = 123;
        assert(read_http_request(sv[1], &out, &out_len) == (i == 0 ? 0 : -1));
        assert(out == NULL && out_len == 0);
        cleanup(sv, out);
    }

    char *out = NULL;
    size_t out_len = 123;
    assert(read_http_request(-1, &out, &out_len) == -1);
    assert(out == NULL && out_len == 0);
    printf("PASS: test_read_failure_cleanup\n");
}

static void test_header_capacity_cleanup(void)
{
    char req[MAX_HEADER_SIZE];
    memset(req, 'A', sizeof(req));
    int sv[2], ret;
    char *out;
    size_t out_len;
    send_and_read(req, sizeof(req), sv, &out, &out_len, &ret);
    assert(ret == -1 && out == NULL && out_len == 0);
    cleanup(sv, out);

    size_t len = (size_t)snprintf(req, sizeof(req), "GET / HTTP/1.1\r\n");
    for (size_t i = 0; i < 1001; i++){
        memcpy(req + len, "X: a\r\n", 6);
        len += 6;
    }
    memcpy(req + len, "\r\n", 2);
    len += 2;
    send_and_read(req, len, sv, &out, &out_len, &ret);
    assert(ret == -1 && out == NULL && out_len == 0);
    cleanup(sv, out);
    printf("PASS: test_header_capacity_cleanup\n");
}

int main(void)
{
    test_post_with_body();
    test_get_no_body();
    test_multiple_headers_with_body();
    test_large_body();
    test_single_header_no_body();
    test_binary_body();
    test_user_agent_without_content_length();
    test_read_failure_cleanup();
    test_header_capacity_cleanup();
    printf("\nAll read_http_request tests passed!\n");
    return 0;
}
