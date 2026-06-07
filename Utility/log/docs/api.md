# API仕様

通常は`utility_log.h`だけをincludeする。

```text
utility_log.h
|-- utility_logger.h
|   `-- utility_log_types.h
|       `-- utility_log_result.h
`-- utility_log_console.h
    `-- utility_log_types.h
```

通常の記録には`UT_LOG_TRACE`から`UT_LOG_FATAL`までのmacroを使う。macroは
対象levelが無効な場合、format引数を評価しない。動的levelを扱う基盤コードのみ
`ut_log_write()`を直接使用する。

起動時に`ut_log_initialize()`を1回呼ぶ。通常処理はLogger pointerを渡さず、
`ut_log_set_console()`、`ut_log_set_ring()`、`ut_log_read()`、
`ut_log_dump()`を使用する。

`ut_logger_*`と`ut_log_write()`は複数Loggerを明示的に扱う低レベルAPIである。
通常の製品Applicationではプロセス既定Logger APIを使用する。

完全な関数契約、引数、戻り値、所有権、thread safetyはDoxygen生成物を正とする。
