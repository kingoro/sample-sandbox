# API仕様

外部向けの集約headerは`include/utility_event.h`。通常はこのheaderだけをincludeする。
QueueまたはDispatcherだけを利用するmoduleは、対応する個別headerを直接include
できる。

| header | 責務 |
| --- | --- |
| `utility_event.h` | 外部公開用の集約header |
| `utility_event_result.h` | 共通result code |
| `utility_event_types.h` | Event記述子と共通ID |
| `utility_event_queue.h` | 固定長FIFO Queue |
| `utility_event_dispatcher.h` | handler登録と同期配送 |

## Event

`ut_event_t`はQueueへ値としてcopyされる。

| field | 内容 |
| --- | --- |
| `id` | applicationが定義するEvent ID |
| `source` | 発行元module ID |
| `payload` | 任意データへの非所有参照。不要なら`NULL` |
| `payload_size` | payloadのbyte数 |

`payload`の解釈、所有権、解放方法はEvent IDごとの契約で定義する。

## Queue API

| API | 動作 |
| --- | --- |
| `ut_event_queue_init` | 呼出側storageでQueueを初期化 |
| `ut_event_queue_push` | Event記述子を末尾へcopy |
| `ut_event_queue_pop` | 先頭Eventを取り出して削除 |
| `ut_event_queue_peek` | 先頭Eventを削除せず参照 |
| `ut_event_queue_clear` | 全Eventを破棄 |
| `ut_event_queue_count` | 現在件数を取得 |
| `ut_event_queue_capacity` | 最大件数を取得 |

## Dispatcher API

| API | 動作 |
| --- | --- |
| `ut_event_dispatcher_init` | 呼出側subscription storageで初期化 |
| `ut_event_subscribe` | Event IDとhandlerを登録 |
| `ut_event_unsubscribe` | 一致する登録を解除 |
| `ut_event_dispatch` | 一致するhandlerを同期実行 |
| `ut_event_subscription_count` | 登録件数を取得 |

## Result code

| code | 意味 |
| --- | --- |
| `UT_EVENT_OK` | 成功 |
| `UT_EVENT_INVALID_ARGUMENT` | NULL、容量0、壊れたcontext |
| `UT_EVENT_EMPTY` | Queueが空 |
| `UT_EVENT_FULL` | Queueまたは登録領域が満杯 |
| `UT_EVENT_NOT_FOUND` | 登録または配送先が存在しない |
| `UT_EVENT_ALREADY_EXISTS` | 同一登録が存在する |
| `UT_EVENT_BUSY` | handler実行中の変更または再帰dispatch |
