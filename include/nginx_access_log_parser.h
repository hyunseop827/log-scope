#ifndef LOGSCOPE_NGINX_ACCESS_LOG_PARSER_H
#define LOGSCOPE_NGINX_ACCESS_LOG_PARSER_H

#include "spring_log_parser.h"

/* Nginx access log 한 줄에서 MVP에 필요한 값만 담는다. */
typedef struct {
    int status;
    char method[LS_METHOD_LEN];
    char path[LS_PATH_LEN];
} NginxAccessLogEntry;

/* 성공하면 1, 기본 access.log 형식이 아니면 0을 반환한다. */
int parse_nginx_access_log_line(const char *line, NginxAccessLogEntry *entry);

#endif
