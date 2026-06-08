#include "log_summary.h"
#include "cli_report.h"

#include <stdio.h>
#include <string.h>

/*
 * Command names
 *
 * 명령어 이름을 바꾸고 싶으면 여기만 수정한다.
 * 예: "spring" -> "boot", "nginx" -> "access"
 */
#define CMD_SPRING "spring"
#define CMD_NGINX "nginx"
#define CMD_COMBINED "combined"

/*
 * Combined command options
 *
 * combined 명령어의 옵션 이름을 바꾸고 싶으면 여기만 수정한다.
 * 예: "--spring" -> "--spring-log", "--nginx" -> "--access-log"
 */
#define OPT_SPRING "--spring"
#define OPT_NGINX "--nginx"

/*
 * CLI usage text
 *
 * 명령어 형식을 바꾸면 이 문구도 같이 수정한다.
 * 실제 파싱 로직은 아래 command 상수와 option 상수를 사용한다.
 */
static void print_usage(const char *program_name)
{
    printf("Usage:\n");
    printf("  %s %s samples/spring.log\n", program_name, CMD_SPRING);
    printf("  %s %s samples/nginx_access.log\n", program_name, CMD_NGINX);
    printf("  %s %s %s samples/spring.log %s samples/nginx_access.log\n",
           program_name,
           CMD_COMBINED,
           OPT_SPRING,
           OPT_NGINX);
}

/* spring 명령어: Spring log 파일을 분석하고 결과를 출력한다. */
static int run_spring_command(const char *path)
{
    SpringLogSummary summary;

    if (!summarize_spring_log_file(path, &summary)) {
        fprintf(stderr, "Failed to read Spring log: %s\n", path);
        spring_log_summary_free(&summary);
        return 1;
    }

    print_spring_log_summary(&summary);
    spring_log_summary_free(&summary);

    return 0;
}

/* nginx 명령어: Nginx access log 파일을 분석하고 결과를 출력한다. */
static int run_nginx_command(const char *path)
{
    NginxAccessLogSummary summary;

    if (!summarize_nginx_access_log_file(path, &summary)) {
        fprintf(stderr, "Failed to read Nginx access log: %s\n", path);
        nginx_access_log_summary_free(&summary);
        return 1;
    }

    print_nginx_access_log_summary(&summary);
    nginx_access_log_summary_free(&summary);

    return 0;
}

/*
 * combined 명령어 옵션을 읽는다.
 * argv 예시:
 *   logscope combined --spring samples/spring.log --nginx samples/nginx_access.log
 */
static int parse_combined_args(int argc, char **argv, const char **spring_log_path, const char **nginx_access_log_path)
{
    int i;

    *spring_log_path = NULL;
    *nginx_access_log_path = NULL;

    for (i = 2; i < argc; i++) {
        if (strcmp(argv[i], OPT_SPRING) == 0 && i + 1 < argc) {
            *spring_log_path = argv[++i];
        } else if (strcmp(argv[i], OPT_NGINX) == 0 && i + 1 < argc) {
            *nginx_access_log_path = argv[++i];
        } else {
            return 0;
        }
    }

    return *spring_log_path != NULL && *nginx_access_log_path != NULL;
}

/* combined 명령어: 두 파일을 각각 분석한 뒤, 공통 기준으로 비교해서 출력한다. */
static int run_combined_command(const char *spring_log_path, const char *nginx_access_log_path)
{
    SpringLogSummary spring_summary;
    NginxAccessLogSummary nginx_access_summary;

    if (!summarize_spring_log_file(spring_log_path, &spring_summary)) {
        fprintf(stderr, "Failed to read Spring log: %s\n", spring_log_path);
        spring_log_summary_free(&spring_summary);
        return 1;
    }

    if (!summarize_nginx_access_log_file(nginx_access_log_path, &nginx_access_summary)) {
        fprintf(stderr, "Failed to read Nginx access log: %s\n", nginx_access_log_path);
        nginx_access_log_summary_free(&nginx_access_summary);
        spring_log_summary_free(&spring_summary);
        return 1;
    }

    print_combined_log_summary(&spring_summary, &nginx_access_summary);

    nginx_access_log_summary_free(&nginx_access_summary);
    spring_log_summary_free(&spring_summary);

    return 0;
}

/*
 * 프로그램 진입점이다.
 * argv[1]에 들어온 명령어 이름을 보고 spring/nginx/combined 중 하나로 분기한다.
 */
int main(int argc, char **argv)
{
    const char *spring_log_path;
    const char *nginx_access_log_path;

    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    if (strcmp(argv[1], CMD_SPRING) == 0) {
        if (argc != 3) {
            print_usage(argv[0]);
            return 1;
        }

        return run_spring_command(argv[2]);
    }

    if (strcmp(argv[1], CMD_NGINX) == 0) {
        if (argc != 3) {
            print_usage(argv[0]);
            return 1;
        }

        return run_nginx_command(argv[2]);
    }

    if (strcmp(argv[1], CMD_COMBINED) == 0) {
        if (!parse_combined_args(argc, argv, &spring_log_path, &nginx_access_log_path)) {
            print_usage(argv[0]);
            return 1;
        }

        return run_combined_command(spring_log_path, nginx_access_log_path);
    }

    print_usage(argv[0]);
    return 1;
}
