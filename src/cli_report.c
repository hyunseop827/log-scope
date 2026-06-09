#include "cli_report.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

/* counter 안에 있는 count 값을 모두 더한다. */
static int total_counter_count(const Counter *counter)
{
    int total = 0;
    size_t i;

    for (i = 0; i < counter->size; i++) {
        total += counter->items[i].count;
    }

    return total;
}

/* method + path + status 형태의 에러 route 집계를 출력한다. */
static void print_route_counter(const Counter *counter)
{
    size_t i;

    if (counter->size == 0) {
        printf("(none)\n");
        return;
    }

    printf("%-6s %-32s %-7s %s\n", "method", "path", "status", "count");

    for (i = 0; i < counter->size; i++) {
        char key[LS_KEY_LEN];
        char *parts[3];

        strncpy(key, counter->items[i].key, sizeof(key) - 1);
        key[sizeof(key) - 1] = '\0';

        if (split_key(key, parts, 3) == 3) {
            printf("%-6s %-32s %-7s %d\n", parts[0], parts[1], parts[2], counter->items[i].count);
        }
    }
}

static int is_error_level(const char *level)
{
    return strcmp(level, "ERROR") == 0;
}

static int is_warn_level(const char *level)
{
    return strcmp(level, "WARN") == 0;
}

static int is_other_level(const char *level)
{
    return !is_error_level(level) && !is_warn_level(level);
}

static int is_target_level(const char *entry_level, const char *target_level)
{
    if (strcmp(target_level, "OTHER") == 0) {
        return is_other_level(entry_level);
    }

    return strcmp(entry_level, target_level) == 0;
}

/* Spring API error 집계에서 특정 level의 count만 더한다. */
static int count_api_errors_by_level(const Counter *counter, const char *level)
{
    int total = 0;
    size_t i;

    for (i = 0; i < counter->size; i++) {
        char key[LS_KEY_LEN];
        char *parts[6];

        strncpy(key, counter->items[i].key, sizeof(key) - 1);
        key[sizeof(key) - 1] = '\0';

        if (split_key(key, parts, 6) == 6 && is_target_level(parts[0], level)) {
            total += counter->items[i].count;
        }
    }

    return total;
}

/* Spring 단독 분석에서 특정 level의 API + ErrorCode + message 조합을 출력한다. */
static void print_api_error_counter_by_level(const Counter *counter, const char *level)
{
    size_t i;
    int printed = 0;

    if (counter->size == 0) {
        printf("(none)\n");
        return;
    }

    printf("%-6s %-32s %-7s %-12s %-32s %s\n", "method", "path", "status", "code", "message", "count");

    for (i = 0; i < counter->size; i++) {
        char key[LS_KEY_LEN];
        char *parts[6];

        strncpy(key, counter->items[i].key, sizeof(key) - 1);
        key[sizeof(key) - 1] = '\0';

        if (split_key(key, parts, 6) == 6 && is_target_level(parts[0], level)) {
            printf(
                "%-6s %-32s %-7s %-12s %-32s %d\n",
                parts[1],
                parts[2],
                parts[3],
                parts[4],
                parts[5],
                counter->items[i].count);
            printed = 1;
        }
    }

    if (!printed) {
        printf("(none)\n");
    }
}

/* --all-fields 옵션에서 원본 key=value 전체를 보여준다. */
static void print_raw_field_counter(const Counter *counter)
{
    size_t i;

    if (counter->size == 0) {
        printf("(none)\n");
        return;
    }

    printf("%-7s %s\n", "count", "key_values");

    for (i = 0; i < counter->size; i++) {
        printf("%-7d %s\n", counter->items[i].count, counter->items[i].key);
    }
}

/* Spring route_code가 특정 Nginx route와 같은 method/path/status인지 확인한다. */
static int is_same_route(char **spring_parts, char **nginx_parts)
{
    return strcmp(spring_parts[0], nginx_parts[0]) == 0 &&
           strcmp(spring_parts[1], nginx_parts[1]) == 0 &&
           strcmp(spring_parts[2], nginx_parts[2]) == 0;
}

/*
 * Nginx 에러 1개와 같은 method/path/status를 가진 Spring ErrorCode를 출력한다.
 * 같은 route/status에 Spring ErrorCode가 여러 개 있으면 여러 줄로 보여준다.
 */
static int print_matching_spring_codes(
    const Counter *spring_route_codes,
    char **nginx_parts,
    int nginx_count)
{
    size_t i;
    int matched = 0;

    for (i = 0; i < spring_route_codes->size; i++) {
        char key[LS_KEY_LEN];
        char *spring_parts[4];

        strncpy(key, spring_route_codes->items[i].key, sizeof(key) - 1);
        key[sizeof(key) - 1] = '\0';

        if (split_key(key, spring_parts, 4) != 4) {
            continue;
        }

        if (!is_same_route(spring_parts, nginx_parts)) {
            continue;
        }

        printf("%-6s %-32s %-7s %-13d %-17s %-14d MATCHED\n",
               nginx_parts[0],
               nginx_parts[1],
               nginx_parts[2],
               nginx_count,
               spring_parts[3],
               spring_route_codes->items[i].count);
        matched = 1;
    }

    return matched;
}

/* Spring route_code와 같은 method/path/status를 가진 Nginx 에러 응답이 있는지 확인한다. */
static int has_nginx_error_route(const Counter *nginx_error_routes, char **spring_parts)
{
    size_t i;

    for (i = 0; i < nginx_error_routes->size; i++) {
        char key[LS_KEY_LEN];
        char *nginx_parts[3];

        strncpy(key, nginx_error_routes->items[i].key, sizeof(key) - 1);
        key[sizeof(key) - 1] = '\0';

        if (split_key(key, nginx_parts, 3) != 3) {
            continue;
        }

        if (is_same_route(spring_parts, nginx_parts)) {
            return 1;
        }
    }

    return 0;
}

/* Spring에는 ErrorCode 로그가 있지만 Nginx 에러 응답에는 없는 항목을 출력한다. */
static int print_spring_only_codes(const SpringLogSummary *spring_summary, const NginxAccessLogSummary *nginx_access_summary)
{
    size_t i;
    int printed = 0;

    for (i = 0; i < spring_summary->route_codes.size; i++) {
        char key[LS_KEY_LEN];
        char *spring_parts[4];

        strncpy(key, spring_summary->route_codes.items[i].key, sizeof(key) - 1);
        key[sizeof(key) - 1] = '\0';

        if (split_key(key, spring_parts, 4) != 4) {
            continue;
        }

        if (has_nginx_error_route(&nginx_access_summary->error_routes, spring_parts)) {
            continue;
        }

        printf("%-6s %-32s %-7s %-13d %-17s %-14d SPRING_ONLY\n",
               spring_parts[0],
               spring_parts[1],
               spring_parts[2],
               0,
               spring_parts[3],
               spring_summary->route_codes.items[i].count);
        printed = 1;
    }

    return printed;
}

/*
 * combined summary에서 Nginx 에러 응답과 Spring ErrorCode를 한 줄 단위로 매핑한다.
 * requestId가 없기 때문에 정확한 요청 1건 매칭이 아니라 method/path/status 기준 집계 비교다.
 */
static void print_combined_error_mapping(const SpringLogSummary *spring_summary, const NginxAccessLogSummary *nginx_access_summary)
{
    size_t i;
    int printed = 0;

    printf("%-6s %-32s %-7s %-13s %-17s %-14s %s\n",
           "method",
           "path",
           "status",
           "nginx_count",
           "spring_code",
           "spring_count",
           "result");

    for (i = 0; i < nginx_access_summary->error_routes.size; i++) {
        char key[LS_KEY_LEN];
        char *nginx_parts[3];
        int nginx_count = nginx_access_summary->error_routes.items[i].count;

        strncpy(key, nginx_access_summary->error_routes.items[i].key, sizeof(key) - 1);
        key[sizeof(key) - 1] = '\0';

        if (split_key(key, nginx_parts, 3) != 3) {
            continue;
        }

        if (print_matching_spring_codes(&spring_summary->route_codes, nginx_parts, nginx_count)) {
            printed = 1;
            continue;
        }

        printf("%-6s %-32s %-7s %-13d %-17s %-14d NGINX_ONLY\n",
               nginx_parts[0],
               nginx_parts[1],
               nginx_parts[2],
               nginx_count,
               "NONE",
               0);
        printed = 1;
    }

    if (print_spring_only_codes(spring_summary, nginx_access_summary)) {
        printed = 1;
    }

    if (!printed) {
        printf("(none)\n");
    }
}

/* method/path/status가 없어서 Nginx와 비교할 수 없는 Spring 로그를 출력한다. */
static void print_missing_match_fields(const Counter *counter)
{
    size_t i;

    if (counter->size == 0) {
        return;
    }

    printf("\nSpring Logs Missing Match Fields:\n");
    printf("%-6s %-32s %-7s %-12s %-32s %-16s %s\n",
           "method",
           "path",
           "status",
           "code",
           "message",
           "missing_fields",
           "count");

    for (i = 0; i < counter->size; i++) {
        char key[LS_KEY_LEN];
        char *parts[6];

        strncpy(key, counter->items[i].key, sizeof(key) - 1);
        key[sizeof(key) - 1] = '\0';

        if (split_key(key, parts, 6) == 6) {
            printf(
                "%-6s %-32s %-7s %-12s %-32s %-16s %d\n",
                parts[0],
                parts[1],
                parts[2],
                parts[3],
                parts[4],
                parts[5],
                counter->items[i].count);
        }
    }
}

/* Spring log 분석 결과를 섹션별로 출력한다. */
void print_spring_log_summary(SpringLogSummary *summary, int show_all_fields)
{
    counter_sort(&summary->api_errors);
    counter_sort(&summary->route_codes);
    counter_sort(&summary->raw_fields);

    printf("Spring Error Report\n\n");
    printf("Total Spring Error Logs: %d\n", summary->total);
    printf("ERROR Logs: %d\n", count_api_errors_by_level(&summary->api_errors, "ERROR"));
    printf("WARN Logs: %d\n", count_api_errors_by_level(&summary->api_errors, "WARN"));

    printf("\nERROR Spring API Errors:\n");
    print_api_error_counter_by_level(&summary->api_errors, "ERROR");

    printf("\nWARN Spring API Errors:\n");
    print_api_error_counter_by_level(&summary->api_errors, "WARN");

    if (count_api_errors_by_level(&summary->api_errors, "OTHER") > 0) {
        printf("\nOTHER Spring API Errors:\n");
        print_api_error_counter_by_level(&summary->api_errors, "OTHER");
    }

    if (show_all_fields) {
        printf("\nRaw Key-Value Fields:\n");
        printf("Raw Fields: all parsed key=value pairs from Spring log lines\n");
        print_raw_field_counter(&summary->raw_fields);
    }
}

/* Nginx access log 분석 결과를 섹션별로 출력한다. */
void print_nginx_access_log_summary(NginxAccessLogSummary *summary)
{
    counter_sort(&summary->error_routes);

    printf("Nginx Access Log Summary\n\n");
    printf("Total Requests: %d\n", summary->total);
    printf("Total Error Responses: %d\n", total_counter_count(&summary->error_routes));
    printf("Default Fields: method, path, status\n");

    printf("\nError Response Details:\n");
    print_route_counter(&summary->error_routes);
}

/* Spring summary와 Nginx summary를 method/path/status 기준으로 비교해서 출력한다. */
void print_combined_log_summary(SpringLogSummary *spring_summary, NginxAccessLogSummary *nginx_access_summary)
{
    counter_sort(&spring_summary->route_codes);
    counter_sort(&spring_summary->missing_match_fields);
    counter_sort(&nginx_access_summary->error_routes);

    printf("Combined Error Mapping\n\n");
    printf("Match Key: method + path + status\n\n");
    print_combined_error_mapping(spring_summary, nginx_access_summary);
    print_missing_match_fields(&spring_summary->missing_match_fields);

    printf("\nConclusion:\n");
    printf("- Nginx 4xx/5xx responses were grouped with Spring ErrorCode logs by method/path/status.\n");
    printf("- This is an aggregate comparison, not request-level correlation.\n");
}
