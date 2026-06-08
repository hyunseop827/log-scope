#include "spring_log_parser.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/* 파싱 전에 구조체를 빈 값으로 초기화해서 이전 값이 남지 않게 한다. */
static void clear_spring_log_entry(SpringLogEntry *entry)
{
    entry->event[0] = '\0';
    entry->code[0] = '\0';
    entry->status = 0;
    entry->method[0] = '\0';
    entry->path[0] = '\0';
    entry->message[0] = '\0';
}

/*
 * key= 뒤의 값을 복사한다.
 * message="Bread sold out"처럼 따옴표가 있으면 닫는 따옴표까지 읽고,
 * code=BREAD_002처럼 따옴표가 없으면 공백 전까지 읽는다.
 */
static void copy_value(char *dest, size_t dest_size, const char *start)
{
    size_t i = 0;

    if (dest_size == 0) {
        return;
    }

    if (*start == '"') {
        start++;
        while (start[i] != '\0' && start[i] != '"' && i + 1 < dest_size) {
            dest[i] = start[i];
            i++;
        }
    } else {
        while (start[i] != '\0' && !isspace((unsigned char)start[i]) && i + 1 < dest_size) {
            dest[i] = start[i];
            i++;
        }
    }

    dest[i] = '\0';
}

/*
 * 한 줄에서 특정 key를 찾고 값을 추출한다.
 * 예: key가 "status="이면 line 안에서 status=409를 찾아 "409"만 복사한다.
 */
static int extract_value(const char *line, const char *key, char *dest, size_t dest_size)
{
    const char *start = strstr(line, key);

    if (start == NULL) {
        return 0;
    }

    start += strlen(key);
    copy_value(dest, dest_size, start);

    return dest[0] != '\0';
}

/*
 * Spring log 한 줄을 SpringLogEntry로 바꾼다.
 * MVP에서는 event/code/status/method/path/message가 모두 있어야 정상 로그로 본다.
 */
int parse_spring_log_line(const char *line, SpringLogEntry *entry)
{
    char status_text[16];

    clear_spring_log_entry(entry);
    status_text[0] = '\0';

    if (!extract_value(line, "event=", entry->event, sizeof(entry->event))) {
        return 0;
    }
    if (!extract_value(line, "code=", entry->code, sizeof(entry->code))) {
        return 0;
    }
    if (!extract_value(line, "status=", status_text, sizeof(status_text))) {
        return 0;
    }
    if (!extract_value(line, "method=", entry->method, sizeof(entry->method))) {
        return 0;
    }
    if (!extract_value(line, "path=", entry->path, sizeof(entry->path))) {
        return 0;
    }
    if (!extract_value(line, "message=", entry->message, sizeof(entry->message))) {
        return 0;
    }

    entry->status = atoi(status_text);
    return entry->status > 0;
}
