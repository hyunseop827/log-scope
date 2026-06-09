#include "spring_log_parser.h"

#include <ctype.h>
#include <string.h>

/* 파싱 전에 구조체를 빈 값으로 초기화해서 이전 값이 남지 않게 한다. */
static void clear_spring_log_entry(SpringLogEntry *entry)
{
    entry->level[0] = '\0';
    entry->code[0] = '\0';
    entry->status[0] = '\0';
    entry->method[0] = '\0';
    entry->path[0] = '\0';
    entry->message[0] = '\0';
    entry->has_level = 0;
    entry->has_code = 0;
    entry->has_status = 0;
    entry->has_method = 0;
    entry->has_path = 0;
    entry->has_message = 0;
    entry->field_count = 0;
}

/* 안전하게 문자열을 복사한다. */
static void copy_text(char *dest, size_t dest_size, const char *src)
{
    if (dest_size == 0) {
        return;
    }

    strncpy(dest, src, dest_size - 1);
    dest[dest_size - 1] = '\0';
}

/* key 이름에 쓸 수 있는 문자만 key로 본다. */
static int is_key_char(char c)
{
    return isalnum((unsigned char)c) || c == '_' || c == '-' || c == '.';
}

/* 로그 앞부분에 있는 WARN/ERROR 토큰을 찾아 level로 저장한다. */
static void infer_log_level(SpringLogEntry *entry, const char *line)
{
    const char *cursor = line;

    while (*cursor != '\0' && *cursor != '=') {
        char token[LS_LEVEL_LEN];
        size_t token_len = 0;

        while (*cursor != '\0' && *cursor != '=' && !isalpha((unsigned char)*cursor)) {
            cursor++;
        }

        while (isalpha((unsigned char)*cursor) && token_len + 1 < sizeof(token)) {
            token[token_len++] = *cursor;
            cursor++;
        }
        token[token_len] = '\0';

        if (strcmp(token, "WARN") == 0 || strcmp(token, "ERROR") == 0) {
            copy_text(entry->level, sizeof(entry->level), token);
            entry->has_level = 1;
            return;
        }
    }
}

/* key=value를 전체 필드 목록에 저장한다. */
static void add_field(SpringLogEntry *entry, const char *key, const char *value, int quoted)
{
    SpringLogField *field;

    if (entry->field_count >= LS_MAX_FIELDS) {
        return;
    }

    field = &entry->fields[entry->field_count];
    copy_text(field->key, sizeof(field->key), key);
    copy_text(field->value, sizeof(field->value), value);
    field->quoted = quoted;
    entry->field_count++;
}

/* LogScope가 기본 분석에 쓰는 5개 필드는 별도 멤버에도 복사한다. */
static void set_known_field(SpringLogEntry *entry, const char *key, const char *value)
{
    if (strcmp(key, "level") == 0) {
        copy_text(entry->level, sizeof(entry->level), value);
        entry->has_level = entry->level[0] != '\0';
    } else if (strcmp(key, "code") == 0) {
        copy_text(entry->code, sizeof(entry->code), value);
        entry->has_code = entry->code[0] != '\0';
    } else if (strcmp(key, "status") == 0) {
        copy_text(entry->status, sizeof(entry->status), value);
        entry->has_status = entry->status[0] != '\0';
    } else if (strcmp(key, "method") == 0) {
        copy_text(entry->method, sizeof(entry->method), value);
        entry->has_method = entry->method[0] != '\0';
    } else if (strcmp(key, "path") == 0) {
        copy_text(entry->path, sizeof(entry->path), value);
        entry->has_path = entry->path[0] != '\0';
    } else if (strcmp(key, "message") == 0) {
        copy_text(entry->message, sizeof(entry->message), value);
        entry->has_message = entry->message[0] != '\0';
    }
}

/*
 * Spring log 한 줄에서 key=value를 전부 찾는다.
 * 따옴표 값은 닫는 따옴표까지 읽고, 일반 값은 공백 전까지 읽는다.
 */
int parse_spring_log_line(const char *line, SpringLogEntry *entry)
{
    const char *cursor = line;

    clear_spring_log_entry(entry);
    infer_log_level(entry, line);

    while (*cursor != '\0') {
        char key[LS_FIELD_KEY_LEN];
        char value[LS_FIELD_VALUE_LEN];
        size_t key_len = 0;
        size_t value_len = 0;
        int quoted = 0;

        while (*cursor != '\0' && !is_key_char(*cursor)) {
            cursor++;
        }

        while (is_key_char(*cursor) && key_len + 1 < sizeof(key)) {
            key[key_len++] = *cursor;
            cursor++;
        }
        key[key_len] = '\0';

        if (key_len == 0 || *cursor != '=') {
            while (*cursor != '\0' && !isspace((unsigned char)*cursor)) {
                cursor++;
            }
            continue;
        }

        cursor++;

        if (*cursor == '"') {
            quoted = 1;
            cursor++;
            while (*cursor != '\0' && *cursor != '"' && value_len + 1 < sizeof(value)) {
                value[value_len++] = *cursor;
                cursor++;
            }
            if (*cursor == '"') {
                cursor++;
            }
        } else {
            while (*cursor != '\0' && !isspace((unsigned char)*cursor) && value_len + 1 < sizeof(value)) {
                value[value_len++] = *cursor;
                cursor++;
            }
        }
        value[value_len] = '\0';

        add_field(entry, key, value, quoted);
        set_known_field(entry, key, value);
    }

    return entry->field_count > 0;
}
