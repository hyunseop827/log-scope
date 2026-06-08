#include "cli_report.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* key 하나와 count 하나로 이루어진 단순 집계를 출력한다. */
static void print_simple_counter(const Counter *counter)
{
    size_t i;

    if (counter->size == 0) {
        printf("(none)\n");
        return;
    }

    for (i = 0; i < counter->size; i++) {
        printf("%-24s %d\n", counter->items[i].key, counter->items[i].count);
    }
}

/*
 * log_summary.c에서는 여러 필드를 탭(\t)으로 합쳐 key 하나로 저장한다.
 * 출력할 때는 다시 탭 기준으로 잘라 method/path/status/code처럼 보여준다.
 */
static int split_key(char *key, char **parts, int max_parts)
{
    int count = 0;
    char *current = key;
    char *tab;

    while (count < max_parts) {
        parts[count++] = current;
        tab = strchr(current, '\t');
        if (tab == NULL) {
            break;
        }

        *tab = '\0';
        current = tab + 1;
    }

    return count;
}

/* method + path 형태의 집계를 보기 좋게 출력한다. */
static void print_path_counter(const Counter *counter)
{
    size_t i;

    if (counter->size == 0) {
        printf("(none)\n");
        return;
    }

    for (i = 0; i < counter->size; i++) {
        char key[LS_KEY_LEN];
        char *parts[2];

        strncpy(key, counter->items[i].key, sizeof(key) - 1);
        key[sizeof(key) - 1] = '\0';

        if (split_key(key, parts, 2) == 2) {
            printf("%-5s %-32s %d\n", parts[0], parts[1], counter->items[i].count);
        }
    }
}

/* method + path + status 형태의 에러 route 집계를 출력한다. */
static void print_route_counter(const Counter *counter)
{
    size_t i;

    if (counter->size == 0) {
        printf("(none)\n");
        return;
    }

    for (i = 0; i < counter->size; i++) {
        char key[LS_KEY_LEN];
        char *parts[3];

        strncpy(key, counter->items[i].key, sizeof(key) - 1);
        key[sizeof(key) - 1] = '\0';

        if (split_key(key, parts, 3) == 3) {
            printf("%-5s %-32s %s count=%d\n", parts[0], parts[1], parts[2], counter->items[i].count);
        }
    }
}

/*
 * combined summary에서 Nginx 에러 route와 같은 Spring ErrorCode만 출력한다.
 * requestId가 없기 때문에 정확한 요청 1건 매칭이 아니라 집계 기준 비교다.
 */
static void print_related_codes(const SpringLogSummary *spring_summary, const NginxAccessLogSummary *nginx_access_summary)
{
    size_t i;
    int printed = 0;

    for (i = 0; i < spring_summary->route_codes.size; i++) {
        char key[LS_KEY_LEN];
        char route_key[LS_KEY_LEN];
        char *parts[4];

        strncpy(key, spring_summary->route_codes.items[i].key, sizeof(key) - 1);
        key[sizeof(key) - 1] = '\0';

        if (split_key(key, parts, 4) != 4) {
            continue;
        }

        /* Spring route_code에서 code만 빼고 Nginx error_route와 같은 key를 만든다. */
        if (snprintf(route_key, sizeof(route_key), "%s\t%s\t%s", parts[0], parts[1], parts[2]) <= 0) {
            continue;
        }

        /* Nginx에서 실제로 4xx/5xx가 나온 route만 관련 ErrorCode로 보여준다. */
        if (counter_get_count(&nginx_access_summary->error_routes, route_key) == 0) {
            continue;
        }

        printf(
            "%-5s %-32s %s %-12s count=%d\n",
            parts[0],
            parts[1],
            parts[2],
            parts[3],
            spring_summary->route_codes.items[i].count);
        printed = 1;
    }

    if (!printed) {
        printf("(none)\n");
    }
}

/* Spring log 분석 결과를 섹션별로 출력한다. */
void print_spring_log_summary(SpringLogSummary *summary)
{
    counter_sort(&summary->events);
    counter_sort(&summary->codes);
    counter_sort(&summary->statuses);
    counter_sort(&summary->paths);
    counter_sort(&summary->route_codes);

    printf("Spring Log Summary\n\n");
    printf("Total Error Logs: %d\n", summary->total);
    if (summary->skipped > 0) {
        printf("Skipped Lines: %d\n", summary->skipped);
    }

    printf("\nBy Event:\n");
    print_simple_counter(&summary->events);

    printf("\nBy Code:\n");
    print_simple_counter(&summary->codes);

    printf("\nBy Status:\n");
    print_simple_counter(&summary->statuses);

    printf("\nBy Path:\n");
    print_path_counter(&summary->paths);
}

/* Nginx access log 분석 결과를 섹션별로 출력한다. */
void print_nginx_access_log_summary(NginxAccessLogSummary *summary)
{
    counter_sort(&summary->statuses);
    counter_sort(&summary->paths);
    counter_sort(&summary->error_routes);

    printf("Nginx Access Log Summary\n\n");
    printf("Total Requests: %d\n", summary->total);
    if (summary->skipped > 0) {
        printf("Skipped Lines: %d\n", summary->skipped);
    }

    printf("\nBy Status:\n");
    print_simple_counter(&summary->statuses);

    printf("\nBy Path:\n");
    print_path_counter(&summary->paths);

    printf("\nError Paths:\n");
    print_route_counter(&summary->error_routes);
}

/* Spring summary와 Nginx summary를 method/path/status 기준으로 비교해서 출력한다. */
void print_combined_log_summary(SpringLogSummary *spring_summary, NginxAccessLogSummary *nginx_access_summary)
{
    counter_sort(&spring_summary->route_codes);
    counter_sort(&nginx_access_summary->error_routes);

    printf("Combined Summary\n\n");

    printf("Nginx Error Responses:\n");
    print_route_counter(&nginx_access_summary->error_routes);

    printf("\nRelated Spring Error Codes:\n");
    print_related_codes(spring_summary, nginx_access_summary);

    printf("\nConclusion:\n");
    printf("- Nginx 4xx/5xx responses were grouped with Spring ErrorCode logs by method/path/status.\n");
    printf("- This is an aggregate comparison, not request-level correlation.\n");

    if (spring_summary->skipped > 0 || nginx_access_summary->skipped > 0) {
        printf("\nSkipped Lines:\n");
        printf("spring.log: %d\n", spring_summary->skipped);
        printf("nginx_access.log: %d\n", nginx_access_summary->skipped);
    }
}
