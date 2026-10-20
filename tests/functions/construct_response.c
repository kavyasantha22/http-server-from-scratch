#include "../../src/http_response.h"
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdlib.h>


void test_get_root(void){
    char *response;
    size_t response_len;

    int ret = construct_response(
        "GET", 3,
        "/", 1,
        "HTTP/1.1", 8,
        NULL, 0,
        NULL, 0,
        NULL, 
        &response, &response_len
    );
    char *expected_response = "HTTP/1.1 200 OK\r\n\r\n";
    assert(ret == 0);
    assert(response_len == strlen(expected_response));
    assert(memcmp(response, expected_response, strlen(expected_response)) == 0);
    printf("Passed get_root.\n");
}


void test_get_echo(void){
    char *response;
    size_t response_len;

    int ret = construct_response(
        "GET", 3,
        "/echo/hello", 11,
        "HTTP/1.1", 8,
        NULL, 0,
        NULL, 0,
        NULL,
        &response, &response_len 
    );
    char *expected_response = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: 5\r\n\r\nhello";
    assert(ret == 0);
    assert(response_len == strlen(expected_response));
    assert(memcmp(response, expected_response, strlen(expected_response)) == 0);
    printf("Passed get_echo.\n");
}


void test_get_user_agent(void){
    char *response;
    size_t response_len;

    int ret = construct_response(
        "GET", 3,
        "/user-agent", 11,
        "HTTP/1.1", 8,
        "User-Agent: curl/8.7.1", 22,
        NULL, 0,
        NULL,
        &response, &response_len
    );
    char *expected_response = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: 10\r\n\r\ncurl/8.7.1";
    assert(ret == 0);
    assert(response_len == strlen(expected_response));
    assert(memcmp(response, expected_response, strlen(expected_response)) == 0);
    printf("Passed get_user_agent.\n");
}


void test_get_files(void){
    const char *file_content = "lorem ipsum";
    size_t file_content_len = strlen(file_content);

    mkdir("test_files", 0755);
    int fd = open("test_files/test_get.txt", O_CREAT | O_WRONLY | O_TRUNC, 0644);
    assert(fd >= 0);
    assert(write(fd, file_content, file_content_len) == (ssize_t)file_content_len);
    close(fd);

    char *response;
    size_t response_len;

    int ret = construct_response(
        "GET", 3,
        "/files/test_get.txt", 19,
        "HTTP/1.1", 8,
        NULL, 0,
        NULL, 0,
        "test_files/",
        &response, &response_len
    );

    char *expected_response = "HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\nContent-Length: 11\r\n\r\nlorem ipsum";
    assert(ret == 0);
    assert(response_len == strlen(expected_response));
    assert(memcmp(response, expected_response, strlen(expected_response)) == 0);
    free(response);
    unlink("test_files/test_get.txt");
    printf("Passed get_files.\n");
}


void test_post_files(void){
    char *response;
    size_t response_len;

    unlink("test_files/test_post.txt");
    assert(open("test_files/test_post.txt", O_RDONLY) < 0);

    char *test_bytes = "anjaymabar";

    int ret = construct_response(
        "POST", 4,
        "/files/test_post.txt", 20,
        "HTTP/1.1", 8,
        NULL, 0,
        test_bytes, strlen(test_bytes),
        "test_files/",
        &response, &response_len
    );

    char *expected_response = "HTTP/1.1 201 CREATED\r\n\r\n";
    assert(ret == 0);
    assert(response_len == strlen(expected_response));
    assert(memcmp(response, expected_response, strlen(expected_response)) == 0);
    
    int fd = open("test_files/test_post.txt", O_RDONLY);
    assert(fd >= 0);
    char buf[1024];
    ssize_t n;
    while ((n = read(fd, buf, sizeof(buf))) > 0) {
        assert(write(STDOUT_FILENO, buf, n) == n);
    }
    assert(memcmp(buf, test_bytes, strlen(test_bytes)) == 0);
    unlink("test_files/test_post.txt");
    printf("Passed post_files.\n");
}


static void test_files_without_directory(void){
    const char *methods[] = {"GET", "POST"};
    const char expected[] = "HTTP/1.1 500 INTERNAL SERVER ERROR\r\n\r\n";
    for (size_t i = 0; i < 2; i++){
        char *response;
        size_t response_len;
        assert(construct_response(methods[i], strlen(methods[i]),
            "/files/manual-test.txt", strlen("/files/manual-test.txt"),
            "HTTP/1.1", 8, NULL, 0, "hello", 5, NULL,
            &response, &response_len) == 0);
        assert(response_len == sizeof(expected) - 1);
        assert(memcmp(response, expected, response_len) == 0);
        free(response);
    }
    printf("Passed files_without_directory.\n");
}

int main(){
    test_get_root();
    test_get_echo();
    test_get_user_agent();
    test_get_files();
    test_post_files();
    test_files_without_directory();
}
