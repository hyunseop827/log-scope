#include "log_summary.h"

#include "spring_log_parser.h"
#include "nginx_access_log_parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* fgets로 읽은 줄 끝의 \n, \r을 제거한다. Windows CRLF도 같이 처리한다. */
static void trim_newline(char *text)
{
    size_t len = strlen(text);

    while (len > 0 && (text[len - 1] == '\n' || text[len - 1] == '\r')) {
        text[len - 1] = '\0';
        len--;
    }
}

/* strncpy 사용 후 마지막 문자를 직접 '\0'으로 맞춰 문자열 끝을 보장한다. */
static void copy_key(char *dest, size_t dest_size, const char *src)
{
    if (dest_size == 0) {
        return;
    }

    strncpy(dest, src, dest_size - 1);
    dest[dest_size - 1] = '\0';
}

/* status 숫자를 counter에서 쓸 문자열 key로 바꾼다. 예: 500 -> "500" */
static int make_status_key(char *dest, size_t dest_size, int status)
{
    return snprintf(dest, dest_size, "%d", status) > 0;
}

/*
 * method + path를 하나의 key로 합친다.
 * 탭(\t)을 구분자로 쓰는 이유는 path 안에 일반 공백이 들어갈 가능성보다 안전하기 때문이다.
 */
static int make_path_key(char *dest, size_t dest_size, const char *method, const char *path)
{
    return snprintf(dest, dest_size, "%s\t%s", method, path) > 0;
}

/* match에서 비교할 method + path + status key를 만든다. */
static int make_route_key(char *dest, size_t dest_size, const char *method, const char *path, int status)
{
    return snprintf(dest, dest_size, "%s\t%s\t%d", method, path, status) > 0;
}

/* Spring ErrorCode까지 포함한 key다. match 비교에서 사용한다. */
static int make_route_code_key(
    char *dest,
    size_t dest_size,
    const char *method,
    const char *path,
    const char *status,
    const char *code)
{
    return snprintf(dest, dest_size, "%s\t%s\t%s\t%s", method, path, status, code) > 0;
}

/* Spring 단독 출력에서 보여줄 API + status + code + message key를 만든다. */
static int make_api_error_key(
    char *dest,
    size_t dest_size,
    const char *level,
    const char *method,
    const char *path,
    const char *status,
    const char *code,
    const char *message)
{
    return snprintf(dest, dest_size, "%s\t%s\t%s\t%s\t%s\t%s", level, method, path, status, code, message) > 0;
}

static const char *field_or_missing(const char *value, int has_value)
{
    return has_value ? value : "-";
}

static int can_match_spring_to_nginx(const SpringLogEntry *entry)
{
    return entry->has_method && entry->has_path && entry->has_status;
}

static int append_text(char *dest, size_t dest_size, const char *text)
{
    size_t used = strlen(dest);
    int written;

    if (used >= dest_size) {
        return 0;
    }

    written = snprintf(dest + used, dest_size - used, "%s", text);
    return written >= 0 && (size_t)written < dest_size - used;
}

/* --all-fields 출력용으로 한 줄의 key=value 전체를 하나의 문자열로 합친다. */
static int make_raw_fields_key(char *dest, size_t dest_size, const SpringLogEntry *entry)
{
    int i;

    dest[0] = '\0';

    for (i = 0; i < entry->field_count; i++) {
        if (i > 0 && !append_text(dest, dest_size, " ")) {
            return 0;
        }
        if (!append_text(dest, dest_size, entry->fields[i].key)) {
            return 0;
        }
        if (!append_text(dest, dest_size, "=")) {
            return 0;
        }
        if (entry->fields[i].quoted && !append_text(dest, dest_size, "\"")) {
            return 0;
        }
        if (!append_text(dest, dest_size, entry->fields[i].value)) {
            return 0;
        }
        if (entry->fields[i].quoted && !append_text(dest, dest_size, "\"")) {
            return 0;
        }
    }

    return dest[0] != '\0';
}

static int make_missing_fields_text(char *dest, size_t dest_size, const SpringLogEntry *entry)
{
    dest[0] = '\0';

    if (!entry->has_method && !append_text(dest, dest_size, "method")) {
        return 0;
    }
    if (!entry->has_path) {
        if (dest[0] != '\0' && !append_text(dest, dest_size, ",")) {
            return 0;
        }
        if (!append_text(dest, dest_size, "path")) {
            return 0;
        }
    }
    if (!entry->has_status) {
        if (dest[0] != '\0' && !append_text(dest, dest_size, ",")) {
            return 0;
        }
        if (!append_text(dest, dest_size, "status")) {
            return 0;
        }
    }

    return dest[0] != '\0';
}

/* match에서 비교 불가능한 Spring 로그를 별도 섹션에 보여주기 위한 key다. */
static int make_missing_match_key(char *dest, size_t dest_size, const SpringLogEntry *entry)
{
    char missing_fields[128];

    if (!make_missing_fields_text(missing_fields, sizeof(missing_fields), entry)) {
        return 0;
    }

    return snprintf(
               dest,
               dest_size,
               "%s\t%s\t%s\t%s\t%s\t%s",
               field_or_missing(entry->method, entry->has_method),
               field_or_missing(entry->path, entry->has_path),
               field_or_missing(entry->status, entry->has_status),
               field_or_missing(entry->code, entry->has_code),
               field_or_missing(entry->message, entry->has_message),
               missing_fields) > 0;
}

/* 공백과 탭만 있는 줄은 빈 줄로 본다. */
static int is_empty_line(const char *line)
{
    while (*line != '\0') {
        if (*line != ' ' && *line != '\t') {
            return 0;
        }
        line++;
    }

    return 1;
}

/* 출력이 보기 좋도록 count가 큰 순서, count가 같으면 key 이름순으로 정렬한다. */
static int compare_count_items(const void *left, const void *right)
{
    const CountItem *a = (const CountItem *)left;
    const CountItem *b = (const CountItem *)right;

    if (a->count != b->count) {
        return b->count - a->count;
    }

    return strcmp(a->key, b->key);
}

/* Counter를 처음 사용할 수 있는 상태로 만든다. */
void counter_init(Counter *counter)
{
    counter->items = NULL;
    counter->size = 0;
    counter->capacity = 0;
}

/* Counter 내부에서 확보한 동적 메모리를 해제한다. */
void counter_free(Counter *counter)
{
    free(counter->items);
    counter->items = NULL;
    counter->size = 0;
    counter->capacity = 0;
}

/*
 * counter에 key를 1회 추가한다.
 * 이미 있으면 count만 올리고, 처음 보는 key면 배열 크기를 늘린 뒤 새 항목을 만든다.
 */
int counter_add(Counter *counter, const char *key)
{
    CountItem *next_items;
    size_t i;
    size_t next_capacity;

    for (i = 0; i < counter->size; i++) {
        if (strcmp(counter->items[i].key, key) == 0) {
            counter->items[i].count++;
            return 1;
        }
    }

    /* 배열이 꽉 찼으면 8, 16, 32... 식으로 capacity를 늘린다. */
    if (counter->size == counter->capacity) {
        next_capacity = counter->capacity == 0 ? 8 : counter->capacity * 2;
        next_items = (CountItem *)realloc(counter->items, next_capacity * sizeof(CountItem));
        if (next_items == NULL) {
            return 0;
        }

        counter->items = next_items;
        counter->capacity = next_capacity;
    }

    copy_key(counter->items[counter->size].key, sizeof(counter->items[counter->size].key), key);
    counter->items[counter->size].count = 1;
    counter->size++;

    return 1;
}

/* match 출력에서 Nginx route와 같은 Spring route가 있는지 찾을 때 사용한다. */
int counter_get_count(const Counter *counter, const char *key)
{
    size_t i;

    for (i = 0; i < counter->size; i++) {
        if (strcmp(counter->items[i].key, key) == 0) {
            return counter->items[i].count;
        }
    }

    return 0;
}

/* counter 내용을 출력 전에 정렬한다. */
void counter_sort(Counter *counter)
{
    qsort(counter->items, counter->size, sizeof(CountItem), compare_count_items);
}

/* Spring 로그 summary가 가진 모든 counter를 초기화한다. */
static void spring_log_summary_init(SpringLogSummary *summary)
{
    summary->total = 0;
    summary->skipped = 0;
    counter_init(&summary->api_errors);
    counter_init(&summary->route_codes);
    counter_init(&summary->raw_fields);
    counter_init(&summary->missing_match_fields);
}

/* Nginx 로그 summary가 가진 모든 counter를 초기화한다. */
static void nginx_access_log_summary_init(NginxAccessLogSummary *summary)
{
    summary->total = 0;
    summary->skipped = 0;
    counter_init(&summary->statuses);
    counter_init(&summary->paths);
    counter_init(&summary->error_routes);
}

/* SpringLogSummary 내부 counter들의 메모리를 정리한다. */
void spring_log_summary_free(SpringLogSummary *summary)
{
    counter_free(&summary->api_errors);
    counter_free(&summary->route_codes);
    counter_free(&summary->raw_fields);
    counter_free(&summary->missing_match_fields);
}

/* NginxAccessLogSummary 내부 counter들의 메모리를 정리한다. */
void nginx_access_log_summary_free(NginxAccessLogSummary *summary)
{
    counter_free(&summary->statuses);
    counter_free(&summary->paths);
    counter_free(&summary->error_routes);
}

/* 파싱된 Spring 로그 한 줄을 여러 기준별 counter에 반영한다. */
static int add_spring_log_entry(SpringLogSummary *summary, const SpringLogEntry *entry)
{
    char api_error_key[LS_KEY_LEN];
    char route_code_key[LS_KEY_LEN];
    char raw_fields_key[LS_KEY_LEN];
    char missing_match_key[LS_KEY_LEN];
    const char *level = field_or_missing(entry->level, entry->has_level);
    const char *method = field_or_missing(entry->method, entry->has_method);
    const char *path = field_or_missing(entry->path, entry->has_path);
    const char *status = field_or_missing(entry->status, entry->has_status);
    const char *code = field_or_missing(entry->code, entry->has_code);
    const char *message = field_or_missing(entry->message, entry->has_message);

    /* 여러 필드를 counter용 문자열 key로 변환한다. */
    if (!make_api_error_key(
            api_error_key,
            sizeof(api_error_key),
            level,
            method,
            path,
            status,
            code,
            message)) {
        return 0;
    }

    /* Spring 단독 출력용 기본 5개 필드 집계다. */
    if (!counter_add(&summary->api_errors, api_error_key)) {
        return 0;
    }

    if (make_raw_fields_key(raw_fields_key, sizeof(raw_fields_key), entry)) {
        if (!counter_add(&summary->raw_fields, raw_fields_key)) {
            return 0;
        }
    }

    if (can_match_spring_to_nginx(entry)) {
        if (!make_route_code_key(route_code_key, sizeof(route_code_key), method, path, status, code)) {
            return 0;
        }
        if (!counter_add(&summary->route_codes, route_code_key)) {
            return 0;
        }
    } else if (make_missing_match_key(missing_match_key, sizeof(missing_match_key), entry)) {
        if (!counter_add(&summary->missing_match_fields, missing_match_key)) {
            return 0;
        }
    } else {
        return 0;
    }

    summary->total++;
    return 1;
}

/* 파싱된 Nginx 로그 한 줄을 여러 기준별 counter에 반영한다. */
static int add_nginx_access_log_entry(NginxAccessLogSummary *summary, const NginxAccessLogEntry *entry)
{
    char status_key[32];
    char path_key[LS_KEY_LEN];
    char route_key[LS_KEY_LEN];

    if (!make_status_key(status_key, sizeof(status_key), entry->status)) {
        return 0;
    }
    if (!make_path_key(path_key, sizeof(path_key), entry->method, entry->path)) {
        return 0;
    }

    if (!counter_add(&summary->statuses, status_key)) {
        return 0;
    }
    if (!counter_add(&summary->paths, path_key)) {
        return 0;
    }

    /* 4xx/5xx만 Error Paths에 따로 모은다. */
    if (entry->status >= 400 && entry->status <= 599) {
        if (!make_route_key(route_key, sizeof(route_key), entry->method, entry->path, entry->status)) {
            return 0;
        }
        if (!counter_add(&summary->error_routes, route_key)) {
            return 0;
        }
    }

    summary->total++;
    return 1;
}

/*
 * Spring log 파일 전체를 분석한다.
 * 파일은 통째로 메모리에 올리지 않고, 한 줄씩 읽어서 바로 집계한다.
 */
int summarize_spring_log_file(const char *path, SpringLogSummary *summary)
{
    FILE *file;
    char line[4096];
    SpringLogEntry entry;

    spring_log_summary_init(summary);

    file = fopen(path, "r");
    if (file == NULL) {
        return 0;
    }

    while (fgets(line, sizeof(line), file) != NULL) {
        trim_newline(line);
        if (is_empty_line(line)) {
            continue;
        }

        if (parse_spring_log_line(line, &entry)) {
            /* 파싱 성공한 줄만 total 집계에 포함한다. */
            if (!add_spring_log_entry(summary, &entry)) {
                fclose(file);
                return 0;
            }
        } else {
            /* 깨진 줄은 프로그램을 멈추지 않고 skipped만 올린다. */
            summary->skipped++;
        }
    }

    fclose(file);
    return 1;
}

/* Nginx access log 파일 전체를 한 줄씩 읽고 집계한다. */
int summarize_nginx_access_log_file(const char *path, NginxAccessLogSummary *summary)
{
    FILE *file;
    char line[4096];
    NginxAccessLogEntry entry;

    nginx_access_log_summary_init(summary);

    file = fopen(path, "r");
    if (file == NULL) {
        return 0;
    }

    while (fgets(line, sizeof(line), file) != NULL) {
        trim_newline(line);
        if (is_empty_line(line)) {
            continue;
        }

        if (parse_nginx_access_log_line(line, &entry)) {
            /* 파싱 성공한 줄만 total 집계에 포함한다. */
            if (!add_nginx_access_log_entry(summary, &entry)) {
                fclose(file);
                return 0;
            }
        } else {
            /* 깨진 줄은 프로그램을 멈추지 않고 skipped만 올린다. */
            summary->skipped++;
        }
    }

    fclose(file);
    return 1;
}
