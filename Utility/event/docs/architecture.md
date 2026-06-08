# 構成と責務

## 目的

Event Utilityは、モジュール間で共有global変数を直接読み書きする代わりに、
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

## Dispatcher

DispatcherはEvent IDとhandlerの対応を固定長配列へ登録する。dispatchは同期処理で、
一致するhandlerを登録slot順に呼び出す。`UT_EVENT_ID_ANY`は全イベントを監視する
ログやtrace用途に使用できる。

handler実行中の登録変更と再帰dispatchは拒否する。handlerは短時間で終了し、
待ち処理や長時間のI/Oを行わない。

実装は`src/utility_event_dispatcher.c`へ分離し、Queueへ依存しない。Event Loopは
Queueから取り出したEventをDispatcherへ渡す利用側の構成要素である。

## State Machine

State MachineはEvent IDで駆動するtable-drivenなflat FSMとして実装する。呼出側は
状態tableと遷移tableをstatic storageなどで定義し、Utilityは現在状態、再入防止flag、
任意のtrace接続だけをcontextとして保持する。heap allocation、RTOS、I/O、clock、
Log Utilityには依存しない。

この形を選ぶ理由:

- 遷移一覧をtableとしてreviewでき、仕様、テスト、coverageの対応を追いやすい
- 状態ごとの大きな`switch`肥大化を避けつつ、状態IDとEvent IDは静的解析しやすい
- 関数pointerはguard、action、entry、exitへ限定し、状態探索や遷移順序は共通化する
- Event UtilityのQueue/Dispatcherと自然に接続でき、handler内の長時間処理を避けやすい
- trace hookを共通化でき、製品ごとのLog、RAM Ring、独自trace sinkへ差し替えられる
- 将来、階層状態やSCXML/code generationを追加する場合も、まずtableを生成対象にできる

最初の実装はflat FSMに限定する。階層状態、並行状態、history state、deferred event、
非同期active objectは直接実装しない。これらは有用だが、最初から入れると共通Utilityの
検証面積が大きくなるため、必要になった時点でtable schemaを拡張する。

## Trace

Traceは`ut_event_trace_t`へ登録したsink callbackへ、Event履歴または状態遷移履歴を
値として通知する。payload本体や状態名の文字列は所有せず、Event ID、source、
状態ID、理由code、payload sizeだけを固定recordへ格納する。

`ut_event_trace_dispatch_handler`はDispatcherの`UT_EVENT_ID_ANY`購読に登録できる。
これにより既存handlerの前後関係をDispatcherの登録順で制御しながら、すべてのEventを
観測できる。

Log Utilityへ保存する場合は`utility_event_trace_log.h`のadapterをsinkとして使う。
Event coreはLoggerを所有せず、Log以外のsinkや製品固有trace sinkも同じcallback契約で
接続できる。

## 呼出側の責務

- QueueとDispatcherへの並行アクセスを必要に応じて直列化する
- ISRから利用する場合は対象環境に合わせた排他または専用adapterを用意する
- payload参照を処理完了まで有効に保つ
- Queue満杯時の再試行、破棄、fault化方針を決める
- Event IDの名前空間とpayload型の対応をapplication側で定義する
- trace sinkの実行時間、保存先、並行アクセスをapplication側で定義する

## 対象外

- thread生成、Mutex、Semaphore
- RTOS Queueの置換
- payload memoryの確保、copy、解放
- 非同期handler実行
- event priority、永続化、network配送
- 階層State Machine、並行状態、history state
- clock、timer、timeout Event生成
- Buffer Pool handleの所有権移譲
- Log Recordの永続化、通信送信、USB/UART出力

## 参考ソース

- W3C SCXML: https://www.w3.org/TR/scxml/
- Quantum Leaps QP/C: https://www.state-machine.com/qpc/
- Practical UML Statecharts in C/C++: https://www.state-machine.com/psicc2/
- C State Machine switch vs struct: https://terurin.work/posts/c-state-machine/c-state-machine-switch-vs-struct/

これらは設計判断の参考であり、本UtilityはSCXML実行器やQP/C互換frameworkではない。
組み込みCの長期保守で重要な、event-driven、明示的な遷移、entry/exit/action、
trace可能性を取り込みつつ、repository内の品質ゲートで検証しやすい小さなC11 APIに
落としている。
