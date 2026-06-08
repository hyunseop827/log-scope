#ifndef LOGSCOPE_LOG_SUMMARY_H
#define LOGSCOPE_LOG_SUMMARY_H

#include <stddef.h>

/* method/path/status를 합친 집계 key까지 담기 위해 넉넉하게 둔다. */
#define LS_KEY_LEN 512

/* 하나의 집계 항목이다. 예: key="500", count=3 */
typedef struct {
    char key[LS_KEY_LEN];
    int count;
} CountItem;

/* CountItem을 동적으로 늘려 가며 저장하는 간단한 counter다. */
typedef struct {
    CountItem *items;
    size_t size;
    size_t capacity;
} Counter;

/* Spring log 분석 결과 전체를 담는다. */
typedef struct {
    int total;
    int skipped;
    Counter events;
    Counter codes;
    Counter statuses;
    Counter paths;
    Counter route_codes;
} SpringLogSummary;

/* Nginx access log 분석 결과 전체를 담는다. */
typedef struct {
    int total;
    int skipped;
    Counter statuses;
    Counter paths;
    Counter error_routes;
} NginxAccessLogSummary;

/* Counter는 내부에서 malloc/realloc을 쓰기 때문에 init/free를 항상 짝으로 사용한다. */
void counter_init(Counter *counter);
void counter_free(Counter *counter);
int counter_add(Counter *counter, const char *key);
int counter_get_count(const Counter *counter, const char *key);
void counter_sort(Counter *counter);

void spring_log_summary_free(SpringLogSummary *summary);
void nginx_access_log_summary_free(NginxAccessLogSummary *summary);

/* 파일을 한 줄씩 읽고, 파싱 성공한 줄만 집계한다. */
int summarize_spring_log_file(const char *path, SpringLogSummary *summary);
int summarize_nginx_access_log_file(const char *path, NginxAccessLogSummary *summary);

#endif
