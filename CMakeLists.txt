cmake_minimum_required(VERSION 3.13)
project(codecrafters-http-server C)

set(CMAKE_C_STANDARD 23)

add_executable(http-server
    src/main.c
    src/http_parser.c
    src/http_response.c
)
