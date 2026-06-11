# 構成と責務

## 目的

Event Foundationは、モジュール間で共有global変数を直接読み書きする代わりに、
「何が起きたか」を固定長Queueへ積み、明示したhandlerへ配送する土台を提供する。

```text
Driver / Timer / Application
            |
            | ut_event_queue_push
            v
       Event Queue
            |
            | pop
            v
       Dispatcher
            |
            v
   Module event handlers
```

## Queue

Queueは呼出側が提供する`ut_event_t`配列をリング状に使用する。heap allocationは
行わない。満杯時には古いイベントを上書きせず`UT_EVENT_FULL`を返すため、
イベント欠落への対応を呼出側が決定できる。

Queueがコピーするのは`ut_event_t`だけであり、`payload`が指すデータ本体は
コピーしない。

実装は`src/utility_event_queue.c`へ分離し、Dispatcherへ依存しない。

## Buffer Pool連携

通常のEvent QueueはEvent記述子を浅くcopyするため、stack上のhandle descriptorを
`payload`へ設定するとcopy後にpointer寿命が切れる。またhandleだけを整数からpointerへ
変換する方式はportable Cの契約として採用できない。

そこでBuffer Pool連携では、Event、Pool参照、handle、length、所有flagを
`ut_event_buffer_message_t`へまとめ、専用QueueがEnvelope全体をmoveする。

```text
Producer owns handle
    |
    | push_move success
    v
Queue owns handle
    |
    | pop_move success
    v
Consumer owns handle
    |
    | release success
    v
Pool owns free storage
```

この形を選ぶ理由:

- 大きなpayload本体をQueueへcopyせず、固定sizeのhandleだけを移動できる
- push失敗、consumer error、shutdown時の解放責務がAPI結果から判定できる
- source Envelopeを空にするため、通常経路での二重releaseを検出しやすい
- Pool操作をcallback化し、既存Memory Buffer、RTOS Pool、専用DMA Poolへ接続できる
- Event handlerにはopaque handleとlengthだけを公開し、arena pointer寿命を渡さない
- generation付きhandleを使うPoolでは、release後のstale handle利用をPool側で拒否できる

Event Foundationはcallback呼出しを直列化しない。同じPoolやQueueを複数実行主体から使う
場合は利用側で排他する。handlerがhandle所有権を別処理へ保持したい場合は、dispatch後に
自動releaseせず、Envelope自体の所有権移譲をApplication契約として追加する。

## Dispatcher

DispatcherはEvent IDとhandlerの対応を固定長配列へ登録する。dispatchは同期処理で、
一致するhandlerを登録slot順に呼び出す。`UT_EVENT_ID_ANY`は全イベントを監視する
ログやtrace用途に使用できる。

handler実行中の登録変更と再帰dispatchは拒否する。handlerは短時間で終了し、
待ち処理や長時間のI/Oを行わない。

実装は`src/utility_event_dispatcher.c`へ分離し、Queueへ依存しない。Event Loopは
Queueから取り出したEventをDispatcherへ渡す利用側の構成要素である。

## 値copy Publisher

小さな完了通知や状態変更payloadを別threadからEvent Loopへ渡す場合、通常Queueの
非所有payload参照だけでは発行元stackの寿命を保証できない。Publisherは呼出側提供の
固定長payload slotへ値copyし、QueueとDispatcherを一つの実行経路として接続する。

```text
Producer
  -> publish_copy
  -> payload slot + Event Queue
  -> dispatch
  -> Dispatcher handler
  -> payload slot release
```

Publisher自体はthread、Mutex、heapを所有しない。複数producerからpublishする場合は
利用側がlock/unlock callbackを設定する。dispatchは単一実行主体から呼び、handlerは
同期実行される。handler中の再publishは許可するが、dispatchの再入は拒否する。

payload storageは利用側が扱うpayload型に必要なalignmentを持たせる。大きな可変長
データやcopyを避けたいデータにはPublisherではなくBuffer Pool所有Envelopeを使う。

## Event Contract Registry

Event ID、payload size範囲、所有方式を不変tableへ集約する。Registryはtableをcopyせず、
初期化時にID重複と矛盾したsize条件を検出する。Executorへ任意接続すると、未登録Eventや
payload条件違反をhandler実行前に拒否できる。

ContractをDispatcherへ埋め込まない理由は、単純な通知用途ではschema検証を不要にでき、
既存Queue/Dispatcherの小さな責務を維持できるためである。所有方式は解放処理ではなく
契約metadataであり、実際の寿命管理はborrow元またはBuffer Envelope所有者が担う。

## Event Executor

Executorは次の順序を1 stepとして固定する。

```text
Timer process
    -> Event Queue
    -> Contract validation
    -> Dispatcher
    -> Metrics
```

1回の処理件数をbudgetで制限し、Eventが継続的に到着しても呼出側が他の処理へ制御を
戻せる。Executor自身はthread、sleep、clock、RTOS Queueを所有しないため、bare metalの
main loop、RTOS task、Linux workerのどこからでも同じcoreを呼べる。

## Metrics

MetricsはQueue high-water mark、publish満杯、配送、未購読、Contract拒否、Timer詰まり、
budget枯渇を固定size counterへ記録する。counterをLogや通信へ直接出力しないため、
実時間処理と診断transportを分離できる。長時間稼働でcounterがwrapしないよう
`UINT64_MAX`で飽和する。

## State Machine

State MachineはEvent IDで駆動するtable-drivenなflat FSMとして実装する。呼出側は
状態tableと遷移tableをstatic storageなどで定義し、Utilityは現在状態、再入防止flag、
任意のtrace接続だけをcontextとして保持する。heap allocation、RTOS、I/O、clock、
Log Foundationには依存しない。

この形を選ぶ理由:

- 遷移一覧をtableとしてreviewでき、仕様、テスト、coverageの対応を追いやすい
- 状態ごとの大きな`switch`肥大化を避けつつ、状態IDとEvent IDは静的解析しやすい
- 関数pointerはguard、action、entry、exitへ限定し、状態探索や遷移順序は共通化する
- Event FoundationのQueue/Dispatcherと自然に接続でき、handler内の長時間処理を避けやすい
- trace hookを共通化でき、製品ごとのLog、RAM Ring、独自trace sinkへ差し替えられる
- 将来、階層状態やSCXML/code generationを追加する場合も、まずtableを生成対象にできる

最初の実装はflat FSMに限定する。階層状態、並行状態、history state、deferred event、
非同期active objectは直接実装しない。これらは有用だが、最初から入れると共通Foundationの
検証面積が大きくなるため、必要になった時点でtable schemaを拡張する。

## Timer Event

Timer Schedulerは呼出側が提供する固定長slot配列へ、Timer ID、Event記述子、deadline、
periodを保持する。clock callbackやhardware timerを内部に持たず、呼出側が取得した
`uint64_t`の単調tickを`ut_event_timer_process`へ渡す。

この形を選ぶ理由:

- wall clock補正や時刻設定変更からtimeout判定を分離できる
- bare metal、各種RTOS、組み込みLinuxで同じTimer coreを使用できる
- hardware ISRやTimer service taskで製品処理を実行せず、Event Queueへ仕事を移せる
- 呼出側storageだけを使うため、heap断片化や実行中allocationを避けられる
- `next_deadline`からtickless sleepやhardware compare値を利用側で決定できる
- clockを任意tickとして注入でき、境界値や長時間経過を単体テストで再現できる

one-shotは発行成功後にslotを解放する。periodic Timerは予定deadlineを基準に次回時刻を
計算するため、process呼出しの遅延が周期へ累積しない。複数周期を通過していた場合は
1 Eventへcoalesceし、次の未来deadlineまで進める。これにより処理復帰直後のEvent burstを
避ける。

Queueが満杯の場合はTimerを消費しない。呼出側はQueueをdrainした後で同じ`now`または
新しい`now`を使って再試行できる。

## Trace

Traceは`ut_event_trace_t`へ登録したsink callbackへ、Event履歴または状態遷移履歴を
値として通知する。payload本体や状態名の文字列は所有せず、Event ID、source、
状態ID、理由code、payload sizeだけを固定recordへ格納する。

`ut_event_trace_dispatch_handler`はDispatcherの`UT_EVENT_ID_ANY`購読に登録できる。
これにより既存handlerの前後関係をDispatcherの登録順で制御しながら、すべてのEventを
観測できる。

Log Foundationへ保存する場合は`utility_event_trace_log.h`のadapterをsinkとして使う。
Event coreはLoggerを所有せず、Log以外のsinkや製品固有trace sinkも同じcallback契約で
接続できる。

Traceの実装範囲は、Eventと状態遷移のrecord生成、sink通知、Log Foundation adapter、
State Machineからの自動通知までとする。永続化、通信送信、USB/UART出力を対象外と
しているのは未実装残ではなく、Application Logと製品保守機能の責務を分離するためで
ある。

## 呼出側の責務

- QueueとDispatcherへの並行アクセスを必要に応じて直列化する
- Publisherのpayload storageへ利用型に必要なalignmentを持たせる
- ISRから利用する場合は対象環境に合わせた排他または専用adapterを用意する
- payload参照を処理完了まで有効に保つ
- Queue満杯時の再試行、破棄、fault化方針を決める
- Event IDの名前空間とpayload型の対応をapplication側で定義する
- Buffer Envelopeを所有する経路の終端でreleaseまたは再移譲する
- Applicationから直接Queueへpublishした結果を必要に応じてMetricsへ通知する
- Executorのbudget、呼出周期、idle/sleep方針を利用環境に合わせて決める
- trace sinkの実行時間、保存先、並行アクセスをapplication側で定義する

## 対象外

- thread生成、Mutex、Semaphore
- RTOS Queueの置換
- Buffer Pool内部のpayload memory割当方式
- Buffer Pool実装、arena管理、handle bit layout
- 非同期handler実行
- event priority、永続化、network配送
- 階層State Machine、並行状態、history state
- hardware timer、clock device、tick変換の所有
- Log Recordの永続化、通信送信、USB/UART出力

## 参考ソース

- W3C SCXML: https://www.w3.org/TR/scxml/
- Quantum Leaps QP/C: https://www.state-machine.com/qpc/
- Practical UML Statecharts in C/C++: https://www.state-machine.com/psicc2/
- C State Machine switch vs struct: https://terurin.work/posts/c-state-machine/c-state-machine-switch-vs-struct/
- POSIX CLOCK_MONOTONIC: https://pubs.opengroup.org/onlinepubs/000095399/functions/clock_getres.html
- Zephyr Timers: https://docs.zephyrproject.org/latest/kernel/services/timing/timers.html
- FreeRTOS Software Timers: https://www.freertos.org/Documentation/02-Kernel/02-Kernel-features/05-Software-timers/01-Software-timers
- Zephyr Workqueue Threads: https://docs.zephyrproject.org/latest/kernel/services/threads/workqueue.html
- CMSIS-RTOS2 Message Queue: https://arm-software.github.io/CMSIS_6/latest/RTOS2/group__CMSIS__RTOS__Message.html
- FreeRTOS Direct-to-Task Notifications: https://www.freertos.org/Documentation/02-Kernel/02-Kernel-features/03-Direct-to-task-notifications/01-Task-notifications
- CMSIS-RTOS2 Memory Pool: https://arm-software.github.io/CMSIS_6/main/RTOS2/group__CMSIS__RTOS__PoolMgmt.html
- Zephyr Memory Slabs: https://docs.zephyrproject.org/latest/kernel/memory_management/slabs.html
- CMSIS-RTOS2 zero-copy mailbox tutorial: https://arm-software.github.io/CMSIS_5/RTOS2/html/rtos2_tutorial.html

これらは設計判断の参考であり、本UtilityはSCXML実行器やQP/C互換frameworkではない。
組み込みCの長期保守で重要な、event-driven、明示的な遷移、entry/exit/action、
trace可能性を取り込みつつ、repository内の品質ゲートで検証しやすい小さなC11 APIに
落としている。
