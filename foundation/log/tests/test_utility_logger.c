/**
 * @file test_utility_logger.c
 * @brief Logger、level制御、RAM Ring、排他処理の単体テスト。
 */
#include "utility_log.h"

#include "test_cases.h"
#include "test_support.h"

#include <string.h>

/** Test callbackの呼出状態。 */
typedef struct test_context {
    /** 擬似clockの現在値。 */
    uint64_t clock_value;
    /** Console callback呼出回数。 */
    size_t console_calls;
    /** lock callback呼出回数。 */
    size_t lock_calls;
    /** unlock callback呼出回数。 */
    size_t unlock_calls;
    /** dump callback呼出回数。 */
    size_t dump_calls;
    /** 最後にConsoleへ渡されたRecord。 */
    ut_log_record_t last_console_record;
} test_context_t;

/**
 * 擬似時刻を1ずつ進めて返す。
 *
 * @param context test context。
 * @return 更新後の擬似時刻。
 */
static uint64_t test_clock(void *context)
{
    test_context_t *test = context;
    return ++test->clock_value;
}

/**
 * Console Recordをtest contextへcopyする。
 *
 * @param record Consoleへ配送されたRecord。
 * @param context test context。
 * @return 常にUT_LOG_OK。
 */
static ut_log_result_t capture_console(
    const ut_log_record_t *record,
    void *context)
{
    test_context_t *test = context;
    test->console_calls++;
    test->last_console_record = *record;
    return UT_LOG_OK;
}

/**
 * Console出力失敗を返すtest callback。
 *
 * @param record Consoleへ配送されたRecord。
 * @param context 未使用。
 * @return 常にUT_LOG_OUTPUT_ERROR。
 */
static ut_log_result_t fail_console(
    const ut_log_record_t *record,
    void *context)
{
    (void)record;
    (void)context;
    return UT_LOG_OUTPUT_ERROR;
}

/**
 * lock呼出回数を記録する。
 *
 * @param context test context。
 */
static void count_lock(void *context)
{
    test_context_t *test = context;
    test->lock_calls++;
}

/**
 * unlock呼出回数を記録する。
 *
 * @param context test context。
 */
static void count_unlock(void *context)
{
    test_context_t *test = context;
    test->unlock_calls++;
}

/**
 * 2件受信後にdumpを中断する。
 *
 * @param record dumpされたRecord。
 * @param context test context。
 * @return 2件目までtrue、それ以降false。
 */
static bool stop_dump_after_two(
    const ut_log_record_t *record,
    void *context)
{
    test_context_t *test = context;
    (void)record;
    test->dump_calls++;
    return test->dump_calls < 2u;
}

/**
 * Logger test用の標準設定を生成する。
 *
 * @param context callbackで使用するtest context。
 * @return 標準test設定。
 */
static ut_logger_config_t make_config(test_context_t *context)
{
    return (ut_logger_config_t){
        .console_write = capture_console,
        .console_context = context,
        .clock = test_clock,
        .clock_context = context,
        .lock = count_lock,
        .unlock = count_unlock,
        .lock_context = context,
        .console_level = UT_LOG_LEVEL_WARN,
        .ring_level = UT_LOG_LEVEL_DEBUG,
        .console_enabled = true,
        .ring_enabled = true
    };
}

/**
 * Level別配送、Record所有、時刻、source情報を検証する。
 *
 * @return 成功時0、失敗時1。
 */
static int test_level_routing_and_record_copy(void)
{
    ut_log_record_t storage[3];
    ut_logger_t logger;
    test_context_t context = {0};
    ut_logger_config_t config = make_config(&context);
    ut_log_record_t record;
    char mutable_message[] = "original";

    CHECK(ut_logger_init(&logger, storage, 3u, &config) == UT_LOG_OK);
    CHECK(!ut_log_is_enabled(&logger, UT_LOG_LEVEL_TRACE));
    CHECK(ut_log_is_enabled(&logger, UT_LOG_LEVEL_DEBUG));

    CHECK(ut_log_write(&logger, UT_LOG_LEVEL_INFO, "CORE", "source.c", 42u,
        "run", "%s-%u", mutable_message, 7u) == UT_LOG_OK);
    mutable_message[0] = 'X';
    CHECK(context.console_calls == 0u);
    CHECK(ut_logger_count(&logger) == 1u);
    CHECK(ut_logger_read(&logger, 0u, &record) == UT_LOG_OK);
    CHECK(record.sequence == 0u);
    CHECK(record.timestamp == 1u);
    CHECK(record.level == UT_LOG_LEVEL_INFO);
    CHECK(record.line == 42u);
    CHECK(strcmp(record.module, "CORE") == 0);
    CHECK(strcmp(record.message, "original-7") == 0);
    CHECK(strcmp(record.file, "source.c") == 0);
    CHECK(strcmp(record.function, "run") == 0);

    CHECK(ut_log_write(&logger, UT_LOG_LEVEL_ERROR, "CORE", "source.c", 43u,
        "run", "failed") == UT_LOG_OK);
    CHECK(context.console_calls == 1u);
    CHECK(strcmp(context.last_console_record.message, "failed") == 0);
    CHECK(context.lock_calls == context.unlock_calls);
    return 0;
}

/**
 * Ring満杯時に最古Recordを上書きし、順序と件数を維持することを検証する。
 *
 * @return 成功時0、失敗時1。
 */
static int test_ring_wraparound_and_dump(void)
{
    ut_log_record_t storage[2];
    ut_logger_t logger;
    test_context_t context = {0};
    ut_logger_config_t config = make_config(&context);
    ut_log_record_t record;

    config.console_enabled = false;
    config.ring_level = UT_LOG_LEVEL_TRACE;
    CHECK(ut_logger_init(&logger, storage, 2u, &config) == UT_LOG_OK);
    CHECK(ut_log_write(&logger, UT_LOG_LEVEL_INFO, "M", "f", 1u, "x",
        "one") == UT_LOG_OK);
    CHECK(ut_log_write(&logger, UT_LOG_LEVEL_INFO, "M", "f", 2u, "x",
        "two") == UT_LOG_OK);
    CHECK(ut_log_write(&logger, UT_LOG_LEVEL_INFO, "M", "f", 3u, "x",
        "three") == UT_LOG_OK);

    CHECK(ut_logger_count(&logger) == 2u);
    CHECK(ut_logger_overwritten_count(&logger) == 1u);
    CHECK(ut_logger_read(&logger, 0u, &record) == UT_LOG_OK);
    CHECK(strcmp(record.message, "two") == 0);
    CHECK(ut_logger_read(&logger, 1u, &record) == UT_LOG_OK);
    CHECK(strcmp(record.message, "three") == 0);
    CHECK(ut_logger_read(&logger, 2u, &record) == UT_LOG_NOT_FOUND);

    CHECK(ut_logger_dump(&logger, stop_dump_after_two, &context) == UT_LOG_OK);
    CHECK(context.dump_calls == 2u);
    CHECK(ut_logger_clear(&logger) == UT_LOG_OK);
    CHECK(ut_logger_count(&logger) == 0u);
    CHECK(ut_logger_overwritten_count(&logger) == 1u);
    return 0;
}

/**
 * 実行時設定と無効macroの引数非評価を検証する。
 *
 * @return 成功時0、失敗時1。
 */
static int test_runtime_configuration_and_macros(void)
{
    ut_log_record_t storage[2];
    ut_logger_t logger;
    test_context_t context = {0};
    ut_logger_config_t config = make_config(&context);
    int side_effect = 0;

    config.console_enabled = false;
    config.ring_enabled = false;
    ut_log_shutdown();
    UT_LOG_DEBUG("TEST", "value=%d", ++side_effect);
    CHECK(side_effect == 0);

    CHECK(ut_log_initialize(&logger, storage, 2u, &config) == UT_LOG_OK);
    CHECK(ut_log_set_ring(true, UT_LOG_LEVEL_DEBUG) == UT_LOG_OK);
    UT_LOG_DEBUG("TEST", "value=%d", ++side_effect);
    CHECK(side_effect == 1);
    CHECK(ut_log_count() == 1u);

    CHECK(ut_log_set_console(true, UT_LOG_LEVEL_ERROR) == UT_LOG_OK);
    UT_LOG_WARN("TEST", "warning");
    CHECK(context.console_calls == 0u);
    UT_LOG_ERROR("TEST", "error");
    CHECK(context.console_calls == 1u);
    CHECK(ut_log_overwritten_count() == 1u);
    CHECK(ut_log_read(0u, &context.last_console_record) == UT_LOG_OK);
    context.dump_calls = 0u;
    CHECK(ut_log_dump(stop_dump_after_two, &context) == UT_LOG_OK);
    CHECK(ut_log_clear() == UT_LOG_OK);
    CHECK(ut_log_count() == 0u);
    ut_log_shutdown();
    CHECK(!ut_log_default_is_enabled(UT_LOG_LEVEL_ERROR));
    CHECK(ut_log_set_ring(
        true, UT_LOG_LEVEL_INFO) == UT_LOG_INVALID_ARGUMENT);
    CHECK(ut_log_read(0u, &context.last_console_record) ==
        UT_LOG_INVALID_ARGUMENT);
    return 0;
}

/**
 * OS callbackを持たない最小構成とConsole失敗を検証する。
 *
 * @return 成功時0、失敗時1。
 */
static int test_optional_callbacks_and_console_failure(void)
{
    ut_log_record_t storage[2];
    ut_logger_t logger;
    ut_log_record_t record;
    ut_logger_config_t config = {
        .console_level = UT_LOG_LEVEL_INFO,
        .ring_level = UT_LOG_LEVEL_INFO,
        .console_enabled = true,
        .ring_enabled = true
    };

    CHECK(ut_logger_init(&logger, storage, 2u, &config) == UT_LOG_OK);
    CHECK(ut_log_is_enabled(&logger, UT_LOG_LEVEL_INFO));
    CHECK(ut_log_write(&logger, UT_LOG_LEVEL_INFO, "CORE", "source.c", 1u,
        "run", "minimal") == UT_LOG_OK);
    CHECK(ut_logger_read(&logger, 0u, &record) == UT_LOG_OK);
    CHECK(record.timestamp == 0u);

    config.console_write = fail_console;
    config.ring_enabled = false;
    CHECK(ut_logger_init(&logger, storage, 2u, &config) == UT_LOG_OK);
    CHECK(ut_log_write(&logger, UT_LOG_LEVEL_INFO, "CORE", "source.c", 2u,
        "run", "failure") == UT_LOG_OUTPUT_ERROR);
    CHECK(ut_logger_count(&logger) == 0u);
    return 0;
}

/**
 * 初期化および公開APIが不正引数を拒否することを検証する。
 *
 * @return 成功時0、失敗時1。
 */
static int test_invalid_arguments(void)
{
    ut_log_record_t storage[1];
    ut_logger_t logger = {0};
    test_context_t context = {0};
    ut_logger_config_t config = make_config(&context);
    ut_log_record_t record;

    CHECK(ut_logger_init(NULL, storage, 1u, &config) ==
        UT_LOG_INVALID_ARGUMENT);
    CHECK(ut_logger_init(&logger, NULL, 1u, &config) ==
        UT_LOG_INVALID_ARGUMENT);
    CHECK(ut_logger_init(&logger, storage, 0u, &config) ==
        UT_LOG_INVALID_ARGUMENT);
    CHECK(ut_logger_init(&logger, storage, 1u, NULL) ==
        UT_LOG_INVALID_ARGUMENT);
    config.console_level = UT_LOG_LEVEL_COUNT;
    CHECK(ut_logger_init(&logger, storage, 1u, &config) ==
        UT_LOG_INVALID_ARGUMENT);
    config = make_config(&context);
    config.ring_level = UT_LOG_LEVEL_COUNT;
    CHECK(ut_logger_init(&logger, storage, 1u, &config) ==
        UT_LOG_INVALID_ARGUMENT);
    config = make_config(&context);
    config.unlock = NULL;
    CHECK(ut_logger_init(&logger, storage, 1u, &config) ==
        UT_LOG_INVALID_ARGUMENT);

    config = make_config(&context);
    CHECK(ut_logger_init(&logger, storage, 1u, &config) == UT_LOG_OK);
    CHECK(ut_logger_set_console(
        NULL, true, UT_LOG_LEVEL_INFO) == UT_LOG_INVALID_ARGUMENT);
    CHECK(ut_logger_set_ring(
        &logger, true, UT_LOG_LEVEL_COUNT) == UT_LOG_INVALID_ARGUMENT);
    CHECK(ut_log_write(&logger, UT_LOG_LEVEL_COUNT, "M", "f", 1u, "x",
        "message") == UT_LOG_INVALID_ARGUMENT);
    CHECK(ut_log_write(&logger, UT_LOG_LEVEL_INFO, NULL, "f", 1u, "x",
        "message") == UT_LOG_INVALID_ARGUMENT);
    CHECK(ut_log_write(&logger, UT_LOG_LEVEL_INFO, "M", NULL, 1u, "x",
        "message") == UT_LOG_INVALID_ARGUMENT);
    CHECK(ut_log_write(&logger, UT_LOG_LEVEL_INFO, "M", "f", 1u, NULL,
        "message") == UT_LOG_INVALID_ARGUMENT);
    CHECK(ut_log_write(&logger, UT_LOG_LEVEL_INFO, "M", "f", 1u, "x",
        NULL) == UT_LOG_INVALID_ARGUMENT);
    CHECK(ut_logger_read(&logger, 0u, NULL) == UT_LOG_INVALID_ARGUMENT);
    CHECK(ut_logger_dump(&logger, NULL, NULL) == UT_LOG_INVALID_ARGUMENT);
    CHECK(ut_logger_clear(NULL) == UT_LOG_INVALID_ARGUMENT);
    CHECK(ut_logger_count(NULL) == 0u);
    CHECK(ut_logger_overwritten_count(NULL) == 0u);
    CHECK(ut_logger_read(&logger, 0u, &record) == UT_LOG_NOT_FOUND);

    logger.storage = NULL;
    CHECK(ut_logger_clear(&logger) == UT_LOG_INVALID_ARGUMENT);
    logger.storage = storage;
    logger.capacity = 0u;
    CHECK(ut_logger_clear(&logger) == UT_LOG_INVALID_ARGUMENT);
    logger.capacity = 1u;
    logger.unlock = NULL;
    CHECK(ut_logger_clear(&logger) == UT_LOG_INVALID_ARGUMENT);
    return 0;
}

int run_utility_logger_tests(void)
{
    CHECK(test_level_routing_and_record_copy() == 0);
    CHECK(test_ring_wraparound_and_dump() == 0);
    CHECK(test_runtime_configuration_and_macros() == 0);
    CHECK(test_optional_callbacks_and_console_failure() == 0);
    CHECK(test_invalid_arguments() == 0);
    return 0;
}
