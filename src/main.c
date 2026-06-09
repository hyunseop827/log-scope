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
#define OPT_ALL_FIELDS "--all-fields"

/* 대화형 입력에서 파일 경로를 담을 버퍼 크기다. */
#define INPUT_BUFFER_LEN 1024

/*
 * CLI usage text
 *
 * 명령어 형식을 바꾸면 이 문구도 같이 수정한다.
 * 실제 파싱 로직은 아래 command 상수와 option 상수를 사용한다.
 */
static void print_usage(const char *program_name)
{
    printf("Usage:\n");
    printf("  %s\n", program_name);
    printf("  %s %s samples/spring.log\n", program_name, CMD_SPRING);
    printf("  %s %s samples/spring.log %s\n", program_name, CMD_SPRING, OPT_ALL_FIELDS);
    printf("  %s %s samples/nginx.log\n", program_name, CMD_NGINX);
    printf("  %s %s %s samples/spring.log %s samples/nginx.log\n",
           program_name,
           CMD_COMBINED,
           OPT_SPRING,
           OPT_NGINX);
}

/* fgets로 입력받은 문자열 끝의 개행 문자를 제거한다. */
static void trim_input_newline(char *text)
{
    size_t len = strlen(text);

    while (len > 0 && (text[len - 1] == '\n' || text[len - 1] == '\r')) {
        text[len - 1] = '\0';
        len--;
    }
}

/* 프롬프트를 출력하고 한 줄을 입력받는다. 입력 실패나 빈 값이면 실패로 본다. */
static int read_prompt_line(const char *prompt, char *buffer, size_t buffer_size)
{
    printf("%s", prompt);
    fflush(stdout);

    if (fgets(buffer, buffer_size, stdin) == NULL) {
        return 0;
    }

    trim_input_newline(buffer);
    return buffer[0] != '\0';
}

static int read_yes_no_prompt(const char *prompt)
{
    char answer[16];

    if (!read_prompt_line(prompt, answer, sizeof(answer))) {
        return 0;
    }

    return strcmp(answer, "y") == 0 || strcmp(answer, "Y") == 0 ||
           strcmp(answer, "yes") == 0 || strcmp(answer, "YES") == 0;
}

/* 인자 없이 실행했을 때 보여줄 메뉴다. */
static void print_interactive_menu(void)
{
    printf("LogScope\n\n");
    printf("1. Spring log only\n");
    printf("2. Nginx access log only\n");
    printf("3. Spring log + Nginx access log\n\n");
}

/* spring 명령어: Spring log 파일을 분석하고 결과를 출력한다. */
static int run_spring_command(const char *path, int show_all_fields)
{
    SpringLogSummary summary;

    if (!summarize_spring_log_file(path, &summary)) {
        fprintf(stderr, "Failed to read Spring log: %s\n", path);
        spring_log_summary_free(&summary);
        return 1;
    }

    print_spring_log_summary(&summary, show_all_fields);
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
 *   logscope combined --spring samples/spring.log --nginx samples/nginx.log
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
 * 인자 없이 실행했을 때의 대화형 모드다.
 * 사용자가 1/2/3 중 하나를 고르고, 필요한 로그 파일 경로를 직접 입력한다.
 */
static int run_interactive_command(void)
{
    char choice[16];
    char spring_log_path[INPUT_BUFFER_LEN];
    char nginx_access_log_path[INPUT_BUFFER_LEN];

    print_interactive_menu();

    if (!read_prompt_line("Select mode (1-3): ", choice, sizeof(choice))) {
        fprintf(stderr, "No mode selected.\n");
        return 1;
    }

    if (strcmp(choice, "1") == 0) {
        int show_all_fields;

        if (!read_prompt_line("Path to Spring log: ", spring_log_path, sizeof(spring_log_path))) {
            fprintf(stderr, "Spring log path is required.\n");
            return 1;
        }

        show_all_fields = read_yes_no_prompt("Show all key-value fields? (y/N): ");
        return run_spring_command(spring_log_path, show_all_fields);
    }

    if (strcmp(choice, "2") == 0) {
        if (!read_prompt_line("Path to Nginx access log: ", nginx_access_log_path, sizeof(nginx_access_log_path))) {
            fprintf(stderr, "Nginx access log path is required.\n");
            return 1;
        }

        return run_nginx_command(nginx_access_log_path);
    }

    if (strcmp(choice, "3") == 0) {
        if (!read_prompt_line("Path to Spring log: ", spring_log_path, sizeof(spring_log_path))) {
            fprintf(stderr, "Spring log path is required.\n");
            return 1;
        }
        if (!read_prompt_line("Path to Nginx access log: ", nginx_access_log_path, sizeof(nginx_access_log_path))) {
            fprintf(stderr, "Nginx access log path is required.\n");
            return 1;
        }

        return run_combined_command(spring_log_path, nginx_access_log_path);
    }

    fprintf(stderr, "Invalid mode: %s\n", choice);
    return 1;
}

/*
 * 프로그램 진입점이다.
 * argv[1]에 들어온 명령어 이름을 보고 spring/nginx/combined 중 하나로 분기한다.
 * 인자가 없으면 1/2/3을 고르는 대화형 모드로 들어간다.
 */
int main(int argc, char **argv)
{
    const char *spring_log_path;
    const char *nginx_access_log_path;

    if (argc == 1) {
        return run_interactive_command();
    }

    if (strcmp(argv[1], CMD_SPRING) == 0) {
        int show_all_fields = 0;

        if (argc == 4) {
            if (strcmp(argv[3], OPT_ALL_FIELDS) != 0) {
                print_usage(argv[0]);
                return 1;
            }
            show_all_fields = 1;
        } else if (argc != 3) {
            print_usage(argv[0]);
            return 1;
        }

        return run_spring_command(argv[2], show_all_fields);
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
