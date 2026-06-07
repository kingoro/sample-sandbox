# 構成と責務

```text
Application
    |
    | UT_LOG_INFO / DEBUG / ...
    v
Process Default Logger
    |
Logger Core
    |-- runtime level filter
    |-- Record formatting and ownership
    |-- sequence / timestamp / source metadata
    |
    |-- Console Sink callback
    `-- RAM Ring Sink
            `-- read / dump / clear
```

Logger Coreは出力先を呼出側APIへ露出しない。同じLog呼出しを、実行時設定に応じて
Console、RAM Ring、または両方へ配送する。

Applicationは起動時に`ut_log_initialize()`へLoggerとstorageを渡す。Utilityは
そのLoggerへの参照をプロセス既定として保持し、通常のLog macro、設定変更、dump
ではLogger引数を要求しない。Logger実体とstorageの所有権はApplicationに残る。

RAM Ringは固定長`ut_log_record_t`配列を呼出側から借りる。heap allocationは
行わない。message、module、source情報はRecord内へcopyし、呼出元memoryの寿命へ
依存しない。

Consoleはcallback interfaceとする。標準C `FILE`向けadapterを提供するが、
製品側は独自Consoleへ差し替えられる。USBや通信処理はLogger callbackとして
直接実装せず、RAM Ring dumpを利用する独立した保守機能として構成する。

thread safetyはOS固有Mutexへ依存させず、lock/unlock callbackを注入する。
callback未設定時は排他されない。dump callback実行中はlockを保持するため、
callbackから同じLoggerを再入してはならない。
