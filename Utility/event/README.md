# Event Utility

Cでイベント駆動処理を構成するための、ドメイン非依存の最小Utility。
独立配布ライブラリではなく、`src/`のCファイルを利用側のビルドへ組み込んで使う。

提供する機能:

- 呼出側提供storageを使う固定長FIFO Event Queue
- Event IDに応じてhandlerを同期実行するDispatcher
- heap、RTOS、thread、I/Oへ依存しないC11実装
- Queue満杯、空、重複登録、再帰dispatchを明示的なresult codeで通知

## 実装範囲

イベント駆動基盤全体のうち、現時点で実装済みなのは次の範囲である。

| 項目 | 状態 | 内容 |
| --- | --- | --- |
| 基本型・エラー体系 | 一部 | Event Utility内の型とresult codeのみ |
| Ring Queue | 実装済み | `ut_event_t`専用の固定長FIFO |
| Event定義 | 実装済み | ID、発行元、非所有payload参照 |
| Dispatcher | 実装済み | Event IDによる同期配送 |
| State Machine | 未実装 | 状態、遷移、entry/exit処理 |
| Timer Event | 未実装 | clock adapter、deadline、timeout発行 |
| Buffer Pool連携 | 未実装 | handle所有権移譲、解放規則 |
| ログ・状態遷移trace | 未実装 | hook、Event履歴、遷移履歴 |

このため、現状をイベント駆動基盤一式とは扱わない。State Machine以降は、
時刻源、Buffer Pool API、trace出力先の契約を定めてから追加する。

## Header構成

通常の利用者は外部公開用の`utility_event.h`だけをincludeする。

```text
utility_event.h
├── utility_event_queue.h
│   ├── utility_event_types.h
│   └── utility_event_result.h
└── utility_event_dispatcher.h
    ├── utility_event_types.h
    └── utility_event_result.h
```

Queueだけを使う低レベルmoduleは`utility_event_queue.h`を直接includeしてもよい。

イベントには印刷データ本体を格納しない。`ut_event_t`は通知情報とpayloadへの参照を
値として保持する。印刷データなどの大きなpayloadは別のBuffer Pool等で管理し、
イベントにはその参照またはhandleを渡す。

## 資料

- [初心者向け はじめてのEvent Utility](README_BEGINNER.md)
- [構成と責務](docs/architecture.md)
- [API仕様](docs/api.md)
- [利用方法](docs/usage.md)
- [テスト方針](docs/quality.md)

## テスト

repository rootで実行する。

```sh
make utility-test
make utility-fuzz-smoke
```
