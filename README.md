# HTTP Server in C

> **⚠️ This project is currently in development. Many more features will be added.**

A lightweight HTTP/1.1 server built from scratch in C using POSIX sockets. The server handles concurrent client connections via multithreading and supports basic request parsing, routing, and response construction — all without external HTTP libraries.

## Features

- **TCP socket server** listening on `0.0.0.0:4221`
- **Multithreaded** — each client connection is handled in its own thread via `pthreads`
- **HTTP/1.1 request parsing** — splits raw requests into request line, headers, and body
- **Route handling:**
  - `GET /` — returns `200 OK`
  - `GET /echo/<string>` — echoes back `<string>` as `text/plain`
  - `GET /user-agent` — returns the client's `User-Agent` header value
  - `GET /files/<filename>` — serves a file from a configurable directory as `application/octet-stream`
  - All other routes return `404 Not Found`
- **Static file serving** from a directory specified via the `--directory` flag

## Project Structure

```
src/
├── main.c            # Entry point, socket setup, accept loop, client thread handler
├── http_parser.c     # HTTP request & request-line parsing
├── http_parser.h
├── http_response.c   # Response construction & routing logic
└── http_response.h
CMakeLists.txt        # CMake build configuration
```

## Prerequisites

- A C compiler with C23 support (e.g. GCC 13+, Clang 16+)
- [CMake](https://cmake.org/) 3.13 or newer
- POSIX-compatible OS (Linux, macOS)

## Building & Running

The included `run.sh` script compiles and runs the server in one step:

```bash
./run.sh
```

To serve files from a specific directory, pass the `--directory` flag:

```bash
./run.sh --directory /path/to/files/
```

> **Note:** If you prefer to build and run manually:
> ```bash
> cmake -B build -S .
> cmake --build ./build
> ./build/http-server --directory /path/to/files/
> ```

The server will start listening on **port 4221**. You can test it with `curl`:

```bash
# Root route
curl -v http://localhost:4221/

# Echo route
curl -v http://localhost:4221/echo/hello

# User-Agent route
curl -v http://localhost:4221/user-agent

# File serving (requires --directory flag)
curl -v http://localhost:4221/files/example.txt
```

## License

This project is for educational purposes.
