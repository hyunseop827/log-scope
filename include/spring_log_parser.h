#ifndef LOGSCOPE_SPRING_LOG_PARSER_H
#define LOGSCOPE_SPRING_LOG_PARSER_H

/* Spring log에서 뽑아낼 문자열 필드의 최대 길이다. */
#define LS_EVENT_LEN 64
#define LS_CODE_LEN 64
#define LS_METHOD_LEN 16
#define LS_PATH_LEN 256
#define LS_MESSAGE_LEN 256

/* Spring 로그 한 줄을 파싱했을 때 담기는 결과 구조체다. */
typedef struct {
    char event[LS_EVENT_LEN];
    char code[LS_CODE_LEN];
    int status;
    char method[LS_METHOD_LEN];
    char path[LS_PATH_LEN];
    char message[LS_MESSAGE_LEN];
} SpringLogEntry;

/* 성공하면 1, 필수 필드가 없거나 깨진 로그면 0을 반환한다. */
int parse_spring_log_line(const char *line, SpringLogEntry *entry);

#endif
