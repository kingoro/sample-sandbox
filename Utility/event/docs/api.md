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
| `utility_event_state_machine.h` | table-driven State Machine |
| `utility_event_timer.h` | 外部tick駆動Timer Scheduler |
| `utility_event_trace.h` | Event・状態遷移trace record生成 |
| `utility_event_trace_log.h` | trace recordのLog Utility adapter |

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

## State Machine API

| API | 動作 |
| --- | --- |
| `ut_event_state_machine_init` | 状態table、遷移table、初期状態で初期化 |
| `ut_event_state_machine_set_trace` | 状態遷移trace出力先を接続または解除 |
| `ut_event_state_machine_dispatch` | Eventを入力し、最初に一致した遷移を実行 |
| `ut_event_state_machine_current_state` | 現在状態IDを取得 |
| `ut_event_state_machine_context` | 初期化時に指定した呼出側contextを取得 |

遷移tableは登録順に評価する。`from_state`には`UT_EVENT_STATE_ID_ANY`、
`event_id`には`UT_EVENT_ID_ANY`を指定できる。guardがNULLまたはtrueを返した最初の
遷移だけを実行する。通常遷移ではexit、action、状態更新、entry、traceの順に進む。
`UT_EVENT_STATE_TRANSITION_INTERNAL`を指定した遷移ではentryとexitを呼ばない。

このAPIは状態名文字列を実行制御には使用しない。名前は診断、table review、
ドキュメント生成の補助情報であり、制御は固定幅の状態IDとEvent IDで行う。

## Timer API

| API | 動作 |
| --- | --- |
| `ut_event_timer_scheduler_init` | 呼出側slot storageで初期化 |
| `ut_event_timer_start` | 未使用IDでone-shot/periodic Timerを開始 |
| `ut_event_timer_restart` | active TimerのEventと時刻設定を置換 |
| `ut_event_timer_cancel` | active Timerを取消してslotを解放 |
| `ut_event_timer_process` | deadline到達EventをQueueへ発行 |
| `ut_event_timer_count` | active Timer件数を取得 |
| `ut_event_timer_next_deadline` | 最短deadlineを取得 |

Timerの時刻単位はUtilityで固定しない。呼出側は同一Schedulerに対して同じ単調clockと
単位を使用する。`delay`は最初の発火まで、`period`は以後の周期を表す。`period == 0`
ならone-shotである。

Queue満杯時は、満杯を検出したTimer以降をactiveなまま残して`UT_EVENT_FULL`を返す。
それ以前に発行済みのTimerは確定済みであり、`out_emitted_count`で件数を取得できる。

## Trace API

| API | 動作 |
| --- | --- |
| `ut_event_trace_init` | trace sink callbackを登録 |
| `ut_event_trace_event` | Event履歴recordをsinkへ通知 |
| `ut_event_trace_state_transition` | 状態遷移履歴recordをsinkへ通知 |
| `ut_event_trace_dispatch_handler` | `UT_EVENT_ID_ANY`購読用のEvent trace handler |
| `ut_event_trace_log_sink` | trace recordをLog Utilityへ出力 |

`ut_event_trace_record_t`はcallback呼出中のみ有効である。payload本体はcopyせず、
Event ID、source、状態ID、理由code、payload sizeだけを記録する。

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
