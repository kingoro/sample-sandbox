/**
 * @file test_utility_log_console.c
 * @brief FILE stream用Console adapterの単体テスト。
 */
#include "utility_log_console.h"

#include "test_cases.h"
#include "test_support.h"

#include <string.h>

/**
 * Level名変換とConsole出力形式を検証する。
 *
 * @return 成功時0、失敗時1。
 */
static int test_level_names_and_file_output(void)
{
    FILE *stream = tmpfile();
    char output[256] = {0};
    bool passed;
    const ut_log_record_t record = {
        .sequence = 1u,
        .timestamp = 123u,
        .level = UT_LOG_LEVEL_WARN,
        .line = 9u,
        .module = "CORE",
        .message = "temperature=80",
        .file = "main.c",
        .function = "run"
    };

    CHECK(stream != NULL);
    passed =
        strcmp(ut_log_level_name(UT_LOG_LEVEL_TRACE), "TRACE") == 0
        && strcmp(ut_log_level_name(UT_LOG_LEVEL_FATAL), "FATAL") == 0
        && strcmp(ut_log_level_name(UT_LOG_LEVEL_COUNT), "UNKNOWN") == 0
        && ut_log_console_write_file(&record, stream) == UT_LOG_OK
        && fflush(stream) == 0
        && fseek(stream, 0L, SEEK_SET) == 0
        && fgets(output, sizeof(output), stream) != NULL
        && strstr(output, "[123] WARN") != NULL
        && strstr(output, "CORE") != NULL
        && strstr(output, "temperature=80") != NULL
        && strstr(output, "main.c:9 run") != NULL;
    CHECK(fclose(stream) == 0);
    CHECK(passed);
    CHECK(ut_log_console_write_file(NULL, NULL) == UT_LOG_INVALID_ARGUMENT);
    return 0;
}

int run_utility_log_console_tests(void)
{
    CHECK(test_level_names_and_file_output() == 0);
    return 0;
}
