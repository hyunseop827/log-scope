# LogScope

LogScope is a C-based CLI tool for checking Spring Boot error logs and Nginx access logs together.

It helps answer three questions:

- Which API path produced which Spring ErrorCode?
- Which API path returned 4xx/5xx at the Nginx layer?
- Did the Nginx error response match a Spring ErrorCode log?

## Requirements

- CMake 3.16+
- C11 compiler
  - macOS: clang
  - Linux: gcc

## Build

```bash
cmake -S . -B build
cmake --build build
```

If CMake is not installed yet:

```bash
brew install cmake
```

## Usage

Interactive mode:

```bash
./build/logscope
```

Command-line mode:

```bash
./build/logscope spring samples/spring.log
./build/logscope spring samples/spring.log --all-fields
./build/logscope nginx samples/nginx.log
./build/logscope combined --spring samples/spring.log --nginx samples/nginx.log
```

## Output

Spring mode shows which API path produced which internal error.
It always focuses on five fields: `method`, `path`, `status`, `code`, and `message`.
If one of those fields is not present in the log line, LogScope prints `MISSING`.

```text
Spring Log Summary

Total Error Logs: 39
Default Fields: method, path, status, code, message

API Error Details:
method path                             status  code         message                          count
GET    /api/users/me                    401     AUTH_002     Expired token                    5
POST   /api/orders                      409     BREAD_002    Bread sold out                   4
MISSING /api/missing-method              400     MISS_001     Method field missing             1
```

Use `--all-fields` to also show every parsed key-value field from the Spring log.

```text
Raw Key-Value Fields:
Raw Fields: all parsed key=value pairs from Spring log lines
count   key_values
5       event=AUTH_ERROR code=AUTH_002 status=401 method=GET path=/api/users/me message="Expired token"
1       code=MISS_001 status=400 path=/api/missing-method message="Method field missing"
```

Nginx mode shows external 4xx/5xx responses.

```text
Nginx Access Log Summary

Total Requests: 45
Total Error Responses: 38
Default Fields: method, path, status

Error Response Details:
method path                             status  count
GET    /api/users/me                    401     5
POST   /api/orders                      409     4
```

Combined mode maps Nginx error responses to Spring ErrorCode logs.

```text
Combined Error Mapping

Match Key: method + path + status

method path                             status  nginx_count   spring_code       spring_count   result
POST   /api/orders                      409     4             BREAD_002         4              MATCHED
GET    /api/admin                       502     1             NONE              0              NGINX_ONLY
POST   /api/internal/batch              500     0             BATCH_001         1              SPRING_ONLY

Spring Logs Missing Match Fields:
method path                             status  code         message                          missing_fields   count
MISSING /api/missing-method              400     MISS_001     Method field missing             method           1
```

Status labels:

- `MATCHED`: Nginx and Spring both have the same `method + path + status`.
- `NGINX_ONLY`: Nginx returned an error response, but no matching Spring ErrorCode log was found.
- `SPRING_ONLY`: Spring logged an ErrorCode, but no matching Nginx error response was found.
- `Spring Logs Missing Match Fields`: Spring log lines that cannot be compared because `method`, `path`, or `status` is missing.

## Supported Log Formats

Spring log lines are parsed as generic `key=value` fields.
For the main summary, these five fields are used:

```text
method path status code message
```

Example:

```text
code=BREAD_002 status=409 method=POST path=/api/orders message="Bread sold out"
```

Nginx access logs use the default combined log request format:

```text
192.168.0.10 - - [08/Jun/2026:20:30:11 +0900] "POST /api/orders HTTP/1.1" 409 532 "-" "Flutter"
```

## Notes

Combined mode is not request-level tracing. It compares aggregate counts by `method + path + status`.
