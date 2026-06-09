#ifndef LOGSCOPE_CLI_REPORT_H
#define LOGSCOPE_CLI_REPORT_H

#include "log_summary.h"

/* 분석된 summary 구조체를 사람이 읽기 쉬운 CLI 출력으로 바꾼다. */
void print_spring_log_summary(SpringLogSummary *summary, int show_all_fields);
void print_nginx_access_log_summary(NginxAccessLogSummary *summary);
void print_combined_log_summary(SpringLogSummary *spring_summary, NginxAccessLogSummary *nginx_access_summary);

#endif
