# はじめてのLog Utility

## 何のためのものか

Applicationが動いている途中で「どこまで処理したか」「どんな値だったか」を後から
調査するための基盤である。プリンタ専用ではなく、sensor、制御装置、家電など別の
組み込みLinux製品にも移植できる。

EEPROMへ保存する製品EventやErrorとは目的が異なる。EEPROM Logは製品仕様上残す
履歴、このUtilityは開発・保守時にApplication内部を診断するための情報である。

## Logを書く

```c
UT_LOG_INFO("SYSTEM", "application started");
UT_LOG_DEBUG("MOTOR", "speed=%u", speed);
UT_LOG_ERROR("DRIVER", "operation failed: code=%d", error);
```

呼出側はConsoleやRingを指定しない。Logger設定に従い、同じRecordが必要な出力先へ
配送される。

Application起動時に、プロセスで1つだけLoggerを初期化する。

```c
static ut_logger_t logger;
static ut_log_record_t storage[128];

void application_initialize(void)
{
    const ut_logger_config_t config = {
        .console_write = ut_log_console_write_file,
        .console_context = stderr,
        .console_level = UT_LOG_LEVEL_DEBUG,
        .ring_level = UT_LOG_LEVEL_INFO,
        .console_enabled = true,
        .ring_enabled = true
    };

    (void)ut_log_initialize(
        &logger, storage, sizeof(storage) / sizeof(storage[0]), &config);
}
```

`logger`はApplicationが所有するが、初期化後の通常処理は存在を意識しない。

## DebugとRelease

Debug時はConsoleとRAM Ringを有効にできる。Release通常時はConsoleを止め、
RAM Ringだけを残せる。保守操作でConsole levelを変更すれば、再buildせずに
表示範囲を変えられる。

```text
Debug:   Console=DEBUG以上、RAM Ring=TRACE以上
Release: Console=OFF、       RAM Ring=INFO以上
調査時:  Console=DEBUG以上、RAM Ring=DEBUG以上
```

## RAM Ring

Log messageは固定長Recordへcopyされる。呼出元の変数や一時文字列が消えても、
保存済みRecordは影響を受けない。容量を超えると最古Recordから上書きする。

`ut_log_read()`または`ut_log_dump()`で実行中に取り出せる。USBやUARTへ
出す処理はUtilityに含めず、製品側がdump callback内で行う。

## Threadから使う場合

複数threadで同じLoggerを共有する場合は、初期化時に製品側のMutexを操作する
lock/unlock callbackを渡す。単一threadなら両方NULLでよい。
