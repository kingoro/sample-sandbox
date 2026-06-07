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

## 呼出側の責務

- QueueとDispatcherへの並行アクセスを必要に応じて直列化する
- ISRから利用する場合は対象環境に合わせた排他または専用adapterを用意する
- payload参照を処理完了まで有効に保つ
- Queue満杯時の再試行、破棄、fault化方針を決める
- Event IDの名前空間とpayload型の対応をapplication側で定義する

## 対象外

- thread生成、Mutex、Semaphore
- RTOS Queueの置換
- payload memoryの確保、copy、解放
- 非同期handler実行
- event priority、永続化、network配送
- State Machineと状態遷移規則
- clock、timer、timeout Event生成
- Buffer Pool handleの所有権移譲
- ログ出力とEvent・状態遷移trace
