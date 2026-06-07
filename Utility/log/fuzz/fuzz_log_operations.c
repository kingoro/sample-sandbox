/**
 * @file fuzz_log_operations.c
 * @brief 任意byte列からLog Utility操作列を実行するfuzz harness。
 */
#include "utility_log.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

/** fuzz実行中の単調増加clock。 */
static uint64_t fuzz_clock_value;

/**
 * fuzz用の単調増加clock。
 *
 * @param context 未使用。
 * @return 更新後の擬似時刻。
 */
static uint64_t fuzz_clock(void *context)
{
    (void)context;
    return ++fuzz_clock_value;
}

/**
 * fuzz不変条件を検査する。
 *
 * @param condition 検査する条件。
 */
static void require_condition(bool condition)
{
    if (!condition) {
        abort();
    }
}

/**
 * byte列をLog操作へ変換して安全性と状態不変条件を検査する。
 *
 * @param data fuzz入力byte列。
 * @param size dataのbyte数。
 * @return 不変条件を満たす場合0。違反時はprocessをabortする。
 */
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    ut_log_record_t storage[8];
    ut_logger_t logger;
    const ut_logger_config_t config = {
        .clock = fuzz_clock,
        .console_level = UT_LOG_LEVEL_INFO,
        .ring_level = UT_LOG_LEVEL_TRACE,
        .ring_enabled = true
    };
    size_t index;

    fuzz_clock_value = 0u;
    require_condition(
        ut_logger_init(&logger, storage, 8u, &config) == UT_LOG_OK);

    for (index = 0u; index < size; index++) {
        const uint8_t value = data[index];
        const ut_log_level_t level =
            (ut_log_level_t)(value % (uint8_t)UT_LOG_LEVEL_COUNT);
        ut_log_record_t record;

        switch ((value / (uint8_t)UT_LOG_LEVEL_COUNT) % 5u) {
        case 0u:
            (void)ut_log_write(&logger, level, "FUZZ", "fuzz.c",
                (uint32_t)index, "operation", "value=%u", value);
            break;
        case 1u:
            (void)ut_logger_set_ring(
                &logger, (value & 1u) != 0u, level);
            break;
        case 2u:
            (void)ut_logger_set_console(
                &logger, (value & 1u) != 0u, level);
            break;
        case 3u:
            (void)ut_logger_read(
                &logger, (size_t)(value % 10u), &record);
            break;
        default:
            (void)ut_logger_clear(&logger);
            break;
        }

        require_condition(logger.count <= logger.capacity);
        require_condition(logger.head < logger.capacity);
    }
    return 0;
}
