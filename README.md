# LogScope

LogScope is a C-based CLI tool for summarizing Spring Boot logs and Nginx access logs.

The v0.1 scope is intentionally small:

- analyze Spring logs
- analyze Nginx access logs
- compare both logs by `method + path + status`

## Requirements
- CMake 3.16+
- C11 compiler
  - macOS: clang
  - Linux: gcc
- Build tools
  - macOS: Xcode Command Line Tools
  - Ubuntu: build-essential

## Build

```bash
cmake -S . -B build
cmake --build build
```

## Usage

Interactive mode:

```bash
./build/logscope
```

Command-line mode:

```bash
./build/logscope spring samples/spring.log
./build/logscope nginx samples/nginx_access.log
./build/logscope combined --spring samples/spring.log --nginx samples/nginx_access.log
```

## Supported Log Formats

Spring log lines should include these `key=value` fields:

```text
event=BUSINESS_ERROR code=BREAD_002 status=409 method=POST path=/api/orders message="Bread sold out"
```

Nginx access logs use the default combined log request format:

```text
192.168.0.10 - - [08/Jun/2026:20:30:11 +0900] "POST /api/orders HTTP/1.1" 409 532 "-" "Flutter"
```

## Notes

Combined mode is not request-level tracing. It compares aggregate counts by `method + path + status`.
