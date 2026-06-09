#ifndef LOGSCOPE_SPRING_LOG_PARSER_H
#define LOGSCOPE_SPRING_LOG_PARSER_H

/* Spring log에서 뽑아낼 문자열 필드의 최대 길이다. */
#define LS_CODE_LEN 64
#define LS_LEVEL_LEN 16
#define LS_METHOD_LEN 16
#define LS_PATH_LEN 256
#define LS_STATUS_LEN 16
#define LS_MESSAGE_LEN 256
#define LS_FIELD_KEY_LEN 64
#define LS_FIELD_VALUE_LEN 256
#define LS_MAX_FIELDS 32

/* key=value 하나를 담는다. quoted는 원래 값이 따옴표로 감싸져 있었는지 표시한다. */
typedef struct {
    char key[LS_FIELD_KEY_LEN];
    char value[LS_FIELD_VALUE_LEN];
    int quoted;
} SpringLogField;

/* Spring 로그 한 줄을 파싱했을 때 담기는 결과 구조체다. */
typedef struct {
    char level[LS_LEVEL_LEN];
    char code[LS_CODE_LEN];
    char status[LS_STATUS_LEN];
    char method[LS_METHOD_LEN];
    char path[LS_PATH_LEN];
    char message[LS_MESSAGE_LEN];
    int has_level;
    int has_code;
    int has_status;
    int has_method;
    int has_path;
    int has_message;
    SpringLogField fields[LS_MAX_FIELDS];
    int field_count;
} SpringLogEntry;

/* 성공하면 1, key=value가 하나도 없으면 0을 반환한다. */
int parse_spring_log_line(const char *line, SpringLogEntry *entry);

#endif
