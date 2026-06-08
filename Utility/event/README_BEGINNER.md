# はじめてのEvent Utility

この資料は、Cの経験はあるがイベント駆動やファームウェア開発には慣れていない
開発者を対象とする。

## 何を解決するものか

複数moduleがglobal変数を直接読み書きすると、値がいつ、どこで、なぜ変わったかを
追いにくくなる。

```c
/* どのmoduleからでも変更でき、変更理由を追跡しにくい。 */
extern int print_requested;
```

Event Utilityでは、「印刷要求が発生した」という事実をEventとしてQueueへ積む。

```text
要求を発行するmodule
        |
        | PRINT_REQUESTED Event
        v
   Event Queue
        |
        v
   Event Dispatcher
        |
        v
 印刷moduleのhandler
```

Eventを受け取ったmoduleだけが自分のcontextを変更する。これにより、別moduleから
内部状態を直接書き換える経路を減らせる。

## Event、Queue、Dispatcherの違い

### Event

Eventは「何が起きたか」を表す小さな記述子である。

```c
typedef struct ut_event {
    uint32_t id;
    uint32_t source;
    const void *payload;
    size_t payload_size;
} ut_event_t;
```

- `id`: 何が起きたか
- `source`: どのmoduleが発行したか
- `payload`: 必要な追加データへの参照
- `payload_size`: 追加データのbyte数

### Event Queue

QueueはEventを発生順に一時保存する。実装は固定長リングキューであり、heapを
使用しない。

```text
head                                  tail
  |                                     |
  v                                     v
[受信完了][印刷要求][送信完了][ 空き ][ 空き ]
```

Queueが満杯になった場合、古いEventを上書きせず`UT_EVENT_FULL`を返す。呼出側は
再試行、破棄、fault遷移などの方針を決める。

### Dispatcher

Dispatcherは、Queueから取り出したEventを対応するhandlerへ同期的に渡す。

```text
PRINT_REQUESTED  -> print_request_handler
SEND_COMPLETED   -> send_complete_handler
TIMEOUT          -> timeout_handler
```

handlerは`ut_event_dispatch`を呼び出したthread上で、その場で実行される。
新しいthreadは作成されない。

## 印刷データ本体との違い

Event Queueは印刷データ本体を保存するリングバッファではない。

```text
Buffer Pool
└── 印刷データ本体を保持

Event Queue
└── 「データ準備完了」という通知とデータへの参照を保持
```

`ut_event_queue_push`がcopyするのは`ut_event_t`だけである。`payload`が指すデータ
本体はcopyしない。

```c
const ut_event_t event = {
    APP_EVENT_PRINT_DATA_READY,
    MODULE_RECEIVER,
    print_data,
    print_data_size
};

result = ut_event_queue_push(&queue, &event);
```

この例では、handlerの処理が終わるまで`print_data`を変更・解放してはならない。
非同期に保持する印刷データはBuffer Poolなどへcopyして寿命を管理する。

## 最小構成

### 1. storageとcontextを用意する

```c
#include "utility_event.h"

static ut_event_t event_storage[16];
static ut_event_queue_t event_queue;

static ut_event_subscription_t subscription_storage[8];
static ut_event_dispatcher_t dispatcher;
```

### 2. 初期化する

```c
ut_event_result_t app_event_init(void)
{
    ut_event_result_t result;

    result = ut_event_queue_init(
        &event_queue, event_storage, 16u);
    if (result != UT_EVENT_OK) {
        return result;
    }

    return ut_event_dispatcher_init(
        &dispatcher, subscription_storage, 8u);
}
```

### 3. handlerを登録する

```c
enum {
    APP_EVENT_PRINT_REQUESTED = 1u
};

static void on_print_requested(
    const ut_event_t *event,
    void *user_context)
{
    print_context_t *print_context = user_context;

    (void)event;
    print_context->state = PRINT_STATE_STARTING;
}

ut_event_result_t app_register_handlers(print_context_t *print_context)
{
    return ut_event_subscribe(
        &dispatcher,
        APP_EVENT_PRINT_REQUESTED,
        on_print_requested,
        print_context);
}
```

### 4. Eventを発行する

```c
ut_event_result_t app_request_print(void)
{
    const ut_event_t event = {
        APP_EVENT_PRINT_REQUESTED,
        MODULE_APPLICATION,
        NULL,
        0u
    };

    return ut_event_queue_push(&event_queue, &event);
}
```

### 5. Event Loopで処理する

```c
void app_event_loop_step(void)
{
    ut_event_t event;

    if (ut_event_queue_pop(&event_queue, &event) == UT_EVENT_OK) {
        (void)ut_event_dispatch(&dispatcher, &event, NULL);
    }
}
```

この関数をapplicationのmain loopまたはEvent処理taskから繰り返し呼ぶ。

## Threadと割り込み

このUtilityは内部にMutexを持たない。同じQueueやDispatcherを複数threadまたは
割り込みから操作する場合は、利用側で直列化する。

特に、ISRから直接`ut_event_queue_push`してよいかは、対象CPUでの排他方式と
実行時間を検討してから決める。必要ならISR専用adapterを別に用意する。

## handlerで避けること

- 長時間待機する
- 大きなI/Oを同期実行する
- Eventやpayloadのpointerを後で使うために保存する
- 同じDispatcherへ再帰的にdispatchする
- 別moduleの内部contextを直接変更する

handlerはEventを受け取り、自moduleの状態更新や次の短い処理を行って戻る。

## 現在まだない機能

このUtilityに現在含まれるのはEvent、Queue、Dispatcher、State Machine、
Event・状態遷移traceのrecord化とLog Utility adapterである。

- Timer Event
- Buffer Poolとの所有権連携

Timer Event、Buffer Pool連携は未実装であり、必要な契約を定めてから責務別ファイルとして
追加する。

## 検証方法

```sh
# C単体テスト
make utility-test

# GCC静的解析
make utility-static-analysis

# ASan／UBSan付き操作列fuzz smoke
make utility-fuzz-smoke

# CoverageとHTML品質レポート
make quality-report
```

総合レポートは`build/reports/index.html`に生成される。
