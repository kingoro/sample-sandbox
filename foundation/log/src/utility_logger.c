/**
 * @file utility_logger.c
 * @brief Loggerのlevel制御、Record生成、RAM Ring管理実装。
 */
#include "utility_logger.h"

#include <stdio.h>
#include <string.h>

/** Applicationが起動時に登録したプロセス既定Logger。 */
static ut_logger_t *default_logger;

/**
 * Logger contextが初期化済みか確認する。
 *
 * @param logger 確認するLogger。
 * @return 内部状態が有効な場合true。
 */
static bool logger_is_valid(const ut_logger_t *logger)
{
    return logger != NULL
        && logger->storage != NULL
        && logger->capacity > 0u
        && ((logger->lock == NULL && logger->unlock == NULL)
            || (logger->lock != NULL && logger->unlock != NULL));
}

/**
 * Levelが公開範囲内か確認する。
 *
 * @param level 確認するLevel。
 * @return TRACEからFATALの範囲内ならtrue。
 */
static bool level_is_valid(ut_log_level_t level)
{
    return level >= UT_LOG_LEVEL_TRACE && level < UT_LOG_LEVEL_COUNT;
}

/**
 * Record生成APIへ渡された値が有効か確認する。
 *
 * @param logger 確認するLogger。
 * @param level 確認するLevel。
 * @param module module文字列。
 * @param file source file文字列。
 * @param function function文字列。
 * @param format format文字列。
 * @return すべて有効な場合true。
 */
static bool write_arguments_are_valid(
    const ut_logger_t *logger,
    ut_log_level_t level,
    const char *module,
    const char *file,
    const char *function,
    const char *format)
{
    return logger_is_valid(logger)
        && level_is_valid(level)
        && module != NULL
        && file != NULL
        && function != NULL
        && format != NULL;
}

/**
 * 設定済みの場合だけ排他領域へ入る。
 *
 * @param logger 初期化済みLogger。
 */
static void logger_lock(ut_logger_t *logger)
{
    if (logger->lock != NULL) {
        logger->lock(logger->lock_context);
    }
}

/**
 * 設定済みの場合だけ排他領域から出る。
 *
 * @param logger 初期化済みLogger。
 */
static void logger_unlock(ut_logger_t *logger)
{
    if (logger->unlock != NULL) {
        logger->unlock(logger->lock_context);
    }
}

/**
 * 文字列を固定長fieldへNUL終端でcopyする。
 *
 * @param destination copy先。
 * @param capacity copy先のbyte容量。
 * @param source 検査済みのcopy元。
 */
static void copy_text(char *destination, size_t capacity, const char *source)
{
    (void)snprintf(destination, capacity, "%s", source);
}

/**
 * RecordをRAM Ringへ保存し、満杯時は最古Recordを上書きする。
 *
 * @param logger 初期化済みLogger。
 * @param record 保存するRecord。
 */
static void ring_push(ut_logger_t *logger, const ut_log_record_t *record)
{
    size_t index = (logger->head + logger->count) % logger->capacity;

    if (logger->count == logger->capacity) {
        index = logger->head;
        logger->head = (logger->head + 1u) % logger->capacity;
        logger->overwritten_count++;
    } else {
        logger->count++;
    }
    logger->storage[index] = *record;
}

ut_log_result_t ut_logger_init(
    ut_logger_t *logger,
    ut_log_record_t *storage,
    size_t capacity,
    const ut_logger_config_t *config)
{
    if (logger == NULL || storage == NULL || capacity == 0u || config == NULL
        || !level_is_valid(config->console_level)
        || !level_is_valid(config->ring_level)
        || ((config->lock == NULL) != (config->unlock == NULL))) {
        return UT_LOG_INVALID_ARGUMENT;
    }

    *logger = (ut_logger_t){
        .storage = storage,
        .capacity = capacity,
        .console_write = config->console_write,
        .console_context = config->console_context,
        .clock = config->clock,
        .clock_context = config->clock_context,
        .lock = config->lock,
        .unlock = config->unlock,
        .lock_context = config->lock_context,
        .console_level = config->console_level,
        .ring_level = config->ring_level,
        .console_enabled = config->console_enabled,
        .ring_enabled = config->ring_enabled
    };
    return UT_LOG_OK;
}

ut_log_result_t ut_log_initialize(
    ut_logger_t *logger,
    ut_log_record_t *storage,
    size_t capacity,
    const ut_logger_config_t *config)
{
    const ut_log_result_t result =
        ut_logger_init(logger, storage, capacity, config);

    if (result == UT_LOG_OK) {
        default_logger = logger;
    }
    return result;
}

void ut_log_shutdown(void)
{
    default_logger = NULL;
}

bool ut_log_default_is_enabled(ut_log_level_t level)
{
    return ut_log_is_enabled(default_logger, level);
}

ut_log_result_t ut_log_set_console(bool enabled, ut_log_level_t level)
{
    return ut_logger_set_console(default_logger, enabled, level);
}

ut_log_result_t ut_log_set_ring(bool enabled, ut_log_level_t level)
{
    return ut_logger_set_ring(default_logger, enabled, level);
}

size_t ut_log_count(void)
{
    return ut_logger_count(default_logger);
}

uint64_t ut_log_overwritten_count(void)
{
    return ut_logger_overwritten_count(default_logger);
}

ut_log_result_t ut_log_read(
    size_t logical_index,
    ut_log_record_t *out_record)
{
    return ut_logger_read(default_logger, logical_index, out_record);
}

ut_log_result_t ut_log_dump(
    ut_log_dump_fn callback,
    void *context)
{
    return ut_logger_dump(default_logger, callback, context);
}

ut_log_result_t ut_log_clear(void)
{
    return ut_logger_clear(default_logger);
}

ut_log_result_t ut_logger_set_console(
    ut_logger_t *logger,
    bool enabled,
    ut_log_level_t level)
{
    if (!logger_is_valid(logger) || !level_is_valid(level)) {
        return UT_LOG_INVALID_ARGUMENT;
    }
    logger_lock(logger);
    logger->console_enabled = enabled;
    logger->console_level = level;
    logger_unlock(logger);
    return UT_LOG_OK;
}

ut_log_result_t ut_logger_set_ring(
    ut_logger_t *logger,
    bool enabled,
    ut_log_level_t level)
{
    if (!logger_is_valid(logger) || !level_is_valid(level)) {
        return UT_LOG_INVALID_ARGUMENT;
    }
    logger_lock(logger);
    logger->ring_enabled = enabled;
    logger->ring_level = level;
    logger_unlock(logger);
    return UT_LOG_OK;
}

bool ut_log_is_enabled(const ut_logger_t *logger, ut_log_level_t level)
{
    bool enabled;
    ut_logger_t *mutable_logger = (ut_logger_t *)logger;

    if (!logger_is_valid(logger) || !level_is_valid(level)) {
        return false;
    }
    logger_lock(mutable_logger);
    enabled = (logger->console_enabled && logger->console_write != NULL
            && level >= logger->console_level)
        || (logger->ring_enabled && level >= logger->ring_level);
    logger_unlock(mutable_logger);
    return enabled;
}

ut_log_result_t ut_log_write_v(
    ut_logger_t *logger,
    ut_log_level_t level,
    const char *module,
    const char *file,
    uint32_t line,
    const char *function,
    const char *format,
    va_list args)
{
    ut_log_record_t record = {0};
    ut_log_console_write_fn console_write = NULL;
    void *console_context = NULL;
    int format_result;

    if (!write_arguments_are_valid(
            logger, level, module, file, function, format)) {
        return UT_LOG_INVALID_ARGUMENT;
    }

    record.level = level;
    record.line = line;
    copy_text(record.module, sizeof(record.module), module);
    copy_text(record.file, sizeof(record.file), file);
    copy_text(record.function, sizeof(record.function), function);
    format_result = vsnprintf(record.message, sizeof(record.message), format, args);
    if (format_result < 0) {
        return UT_LOG_OUTPUT_ERROR;
    }

    logger_lock(logger);
    record.sequence = logger->next_sequence++;
    if (logger->clock != NULL) {
        record.timestamp = logger->clock(logger->clock_context);
    }
    if (logger->ring_enabled && level >= logger->ring_level) {
        ring_push(logger, &record);
    }
    if (logger->console_enabled && logger->console_write != NULL
        && level >= logger->console_level) {
        console_write = logger->console_write;
        console_context = logger->console_context;
    }
    logger_unlock(logger);

    if (console_write != NULL
        && console_write(&record, console_context) != UT_LOG_OK) {
        return UT_LOG_OUTPUT_ERROR;
    }
    return UT_LOG_OK;
}

ut_log_result_t ut_log_write(
    ut_logger_t *logger,
    ut_log_level_t level,
    const char *module,
    const char *file,
    uint32_t line,
    const char *function,
    const char *format,
    ...)
{
    ut_log_result_t result;
    va_list args;

    va_start(args, format);
    result = ut_log_write_v(
        logger, level, module, file, line, function, format, args);
    va_end(args);
    return result;
}

ut_log_result_t ut_log_write_default(
    ut_log_level_t level,
    const char *module,
    const char *file,
    uint32_t line,
    const char *function,
    const char *format,
    ...)
{
    ut_log_result_t result;
    va_list args;

    va_start(args, format);
    result = ut_log_write_v(
        default_logger, level, module, file, line, function, format, args);
    va_end(args);
    return result;
}

size_t ut_logger_count(const ut_logger_t *logger)
{
    size_t count;
    ut_logger_t *mutable_logger = (ut_logger_t *)logger;

    if (!logger_is_valid(logger)) {
        return 0u;
    }
    logger_lock(mutable_logger);
    count = logger->count;
    logger_unlock(mutable_logger);
    return count;
}

uint64_t ut_logger_overwritten_count(const ut_logger_t *logger)
{
    uint64_t count;
    ut_logger_t *mutable_logger = (ut_logger_t *)logger;

    if (!logger_is_valid(logger)) {
        return 0u;
    }
    logger_lock(mutable_logger);
    count = logger->overwritten_count;
    logger_unlock(mutable_logger);
    return count;
}

ut_log_result_t ut_logger_read(
    ut_logger_t *logger,
    size_t logical_index,
    ut_log_record_t *out_record)
{
    if (!logger_is_valid(logger) || out_record == NULL) {
        return UT_LOG_INVALID_ARGUMENT;
    }
    logger_lock(logger);
    if (logical_index >= logger->count) {
        logger_unlock(logger);
        return UT_LOG_NOT_FOUND;
    }
    *out_record =
        logger->storage[(logger->head + logical_index) % logger->capacity];
    logger_unlock(logger);
    return UT_LOG_OK;
}

ut_log_result_t ut_logger_dump(
    ut_logger_t *logger,
    ut_log_dump_fn callback,
    void *context)
{
    size_t index;

    if (!logger_is_valid(logger) || callback == NULL) {
        return UT_LOG_INVALID_ARGUMENT;
    }
    logger_lock(logger);
    for (index = 0u; index < logger->count; index++) {
        const ut_log_record_t *record =
            &logger->storage[(logger->head + index) % logger->capacity];
        if (!callback(record, context)) {
            break;
        }
    }
    logger_unlock(logger);
    return UT_LOG_OK;
}

ut_log_result_t ut_logger_clear(ut_logger_t *logger)
{
    if (!logger_is_valid(logger)) {
        return UT_LOG_INVALID_ARGUMENT;
    }
    logger_lock(logger);
    logger->head = 0u;
    logger->count = 0u;
    logger_unlock(logger);
    return UT_LOG_OK;
}
