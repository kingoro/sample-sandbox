# API仕様

外部向けの集約headerは`include/utility_event.h`。通常はこのheaderだけをincludeする。
QueueまたはDispatcherだけを利用するmoduleは、対応する個別headerを直接include
できる。

| header | 責務 |
| --- | --- |
| `utility_event.h` | 外部公開用の集約header |
| `utility_event_result.h` | 共通result code |
| `utility_event_types.h` | Event記述子と共通ID |
| `utility_event_buffer.h` | Buffer Pool所有Envelopeとmove Queue |
| `utility_event_contract.h` | Event IDとpayload契約のRegistry |
| `utility_event_queue.h` | 固定長FIFO Queue |
| `utility_event_dispatcher.h` | handler登録と同期配送 |
| `utility_event_executor.h` | Timer、契約検証、同期配送の実行step |
| `utility_event_metrics.h` | Event処理Metrics |
| `utility_event_publisher.h` | payload値copy、Queue、Dispatcher統合 |
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

## Buffer Pool API

| API | 動作 |
| --- | --- |
| `ut_event_buffer_message_create_copy` | Poolへalloc/writeして所有Envelopeを生成 |
| `ut_event_buffer_message_release` | 所有handleをPoolへ返却 |
| `ut_event_buffer_message_event` | Dispatcher用Eventを取得 |
| `ut_event_buffer_message_read` | Envelopeの有効範囲からcopy |
| `ut_event_buffer_queue_init` | 所有Envelope専用Queueを初期化 |
| `ut_event_buffer_queue_push_move` | producerからQueueへ所有権をmove |
| `ut_event_buffer_queue_pop_move` | Queueからconsumerへ所有権をmove |
| `ut_event_buffer_queue_count` | Queue件数を取得 |
| `ut_event_buffer_queue_release_all` | Queue所有handleを全返却 |

`ut_event_buffer_pool_t`はalloc、free、write、read callbackとopaque contextを持つ。
Event UtilityはPool実装、handle bit layout、arena、OSを認識しない。

所有権規則:

| 操作 | 成功時 | 失敗時 |
| --- | --- | --- |
| create_copy | messageが所有 | allocationなし、または内部free済み |
| push_move | Queueが所有 | source messageが所有 |
| pop_move | destinationが所有 | Queueが所有 |
| release | 所有権終了 | messageが所有を維持 |
| release_all | Queueが空 | 失敗位置以降をQueueが所有 |

Envelope内の`event.payload`はEnvelope自身の`buffer_ref`を指す。このためEnvelopeを
通常の`ut_event_queue_t`へ浅くcopyしてはならない。専用move Queueを使用する。

## Contract Registry API

| API | 動作 |
| --- | --- |
| `ut_event_contract_registry_init` | table条件とEvent ID重複を検査して初期化 |
| `ut_event_contract_find` | Event IDのContractを取得 |
| `ut_event_contract_validate` | Event ID、payload pointer、sizeを検証 |

`UT_EVENT_PAYLOAD_NONE`、`UT_EVENT_PAYLOAD_BORROWED`、
`UT_EVENT_PAYLOAD_BUFFER_REF`で所有方式を明示する。Registryは所有方式に基づく解放を
実行せず、Event schemaの検証と診断metadataの提供だけを担当する。

## Executor API

| API | 動作 |
| --- | --- |
| `ut_event_executor_init` | Queue、Dispatcherと任意のTimer/Contract/Metricsを接続 |
| `ut_event_executor_run_once` | Timer処理後、budget件まで検証・同期配送 |

Contract違反EventはQueueから除去して配送しない。購読先なしは`unhandled`として記録し、
残りのEvent処理を継続する。TimerのQueue満杯は既存Queueをdrainした後に
`UT_EVENT_FULL`として呼出側へ返す。

## Metrics API

| API | 動作 |
| --- | --- |
| `ut_event_metrics_init` | counterを0で初期化 |
| `ut_event_metrics_record_publish` | publish結果とQueue件数を記録 |
| `ut_event_metrics_observe_queue` | Queue high-water markを更新 |
| `ut_event_metrics_snapshot` | 現在値を値copy |
| `ut_event_metrics_reset` | 全counterを0へ戻す |

累積counterは`UINT64_MAX`で飽和し、wraparoundしない。Metricsはheap、clock、lock、
出力I/Oを所有しない。

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

## Publisher API

| API | 動作 |
| --- | --- |
| `ut_event_publisher_init` | 呼出側storageと任意の排他callbackで初期化 |
| `ut_event_publisher_subscribe` | 内部Dispatcherへhandlerを登録 |
| `ut_event_publisher_publish_copy` | payloadを固定長slotへcopyしてQueueへ発行 |
| `ut_event_publisher_dispatch` | budget件まで同期配送してpayload slotを解放 |
| `ut_event_publisher_count` | 未配送Event件数を取得 |

Publisherはpayloadを値copyするため、成功後はcopy元の寿命に依存しない。配送handlerが
payloadを構造体pointerとして参照する場合、payload storageにはその型が要求する
alignmentを持たせる。handler callback終了後にpayload pointerを保持してはならない。

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
