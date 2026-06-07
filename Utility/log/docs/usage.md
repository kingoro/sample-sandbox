# 利用方法

```c
#include "utility_log.h"

static ut_log_record_t log_storage[128];
static ut_logger_t logger;

static void initialize_log(void)
{
    const ut_logger_config_t config = {
        .console_write = ut_log_console_write_file,
        .console_context = stderr,
        .clock = product_monotonic_milliseconds,
        .clock_context = NULL,
        .lock = product_log_lock,
        .unlock = product_log_unlock,
        .lock_context = NULL,
        .console_level = UT_LOG_LEVEL_DEBUG,
        .ring_level = UT_LOG_LEVEL_INFO,
        .console_enabled = true,
        .ring_enabled = true
    };

    (void)ut_log_initialize(
        &logger, log_storage,
        sizeof(log_storage) / sizeof(log_storage[0]), &config);
}
```

実行中の設定変更:

```c
(void)ut_log_set_console(false, UT_LOG_LEVEL_DEBUG);
(void)ut_log_set_ring(true, UT_LOG_LEVEL_INFO);
```

保守機能でdumpする場合、callbackはRecordを受け取るだけにし、同じLoggerへLogを
書き戻さない。UARTやUSBへ書く処理の失敗・再試行は保守機能側で管理する。
