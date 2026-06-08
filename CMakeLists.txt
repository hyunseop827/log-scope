cmake_minimum_required(VERSION 3.16)

project(logscope C)

# C11 표준으로 컴파일한다. macOS clang, Linux gcc 모두 무난하게 지원한다.
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# 실행 파일 이름은 logscope이고, 아래 C 파일들을 함께 컴파일한다.
add_executable(logscope
    src/main.c
    src/spring_log_parser.c
    src/nginx_access_log_parser.c
    src/log_summary.c
    src/cli_report.c
)

# include/ 폴더 안의 헤더 파일을 #include "..." 형태로 찾을 수 있게 한다.
target_include_directories(logscope PRIVATE include)
