#include "nginx_access_log_parser.h"

#include <stdio.h>
#include <string.h>

/* 파싱 전에 결과 구조체를 비워서 실패한 파싱의 흔적이 남지 않게 한다. */
static void clear_nginx_access_log_entry(NginxAccessLogEntry *entry)
{
    entry->status = 0;
    entry->method[0] = '\0';
    entry->path[0] = '\0';
}

/*
 * Nginx 기본 combined access.log 한 줄을 파싱한다.
 * 핵심 흐름:
 * 1. 따옴표 안의 request line을 찾는다. 예: "POST /api/orders HTTP/1.1"
 * 2. request line에서 method와 path를 뽑는다.
 * 3. 닫는 따옴표 뒤에 있는 status code를 뽑는다.
 */
int parse_nginx_access_log_line(const char *line, NginxAccessLogEntry *entry)
{
    const char *request_start;
    const char *request_end;
    char request_line[512];
    char protocol[32];
    size_t request_len;

    clear_nginx_access_log_entry(entry);

    /* 첫 번째 따옴표가 request line의 시작이다. */
    request_start = strchr(line, '"');
    if (request_start == NULL) {
        return 0;
    }

    /* 두 번째 따옴표가 request line의 끝이다. */
    request_start++;
    request_end = strchr(request_start, '"');
    if (request_end == NULL) {
        return 0;
    }

    request_len = (size_t)(request_end - request_start);
    if (request_len >= sizeof(request_line)) {
        return 0;
    }

    memcpy(request_line, request_start, request_len);
    request_line[request_len] = '\0';
    protocol[0] = '\0';

    /* protocol은 지금 쓰지 않지만, request line 형식을 안정적으로 읽기 위해 받는다. */
    if (sscanf(request_line, "%15s %255s %31s", entry->method, entry->path, protocol) < 2) {
        return 0;
    }

    /* request line이 끝난 뒤의 숫자가 HTTP status다. */
    if (sscanf(request_end + 1, "%d", &entry->status) != 1) {
        return 0;
    }

    return entry->status > 0;
}
