# Log Utility

組み込みLinux製品のApplication Logを扱う、製品・domain非依存のC基盤Utility。
EEPROMへ残す製品Event/Error Logとは別系統で、実行中の診断情報を管理する。

## 提供機能

- `TRACE`、`DEBUG`、`INFO`、`WARN`、`ERROR`、`FATAL`のlevel
- Console出力と固定長RAM Ring保存の個別level・有効状態
- 呼出側所有storageへの整形済みRecord copy
- RAM Ring満杯時の最古Record上書きと上書き件数
- 実行中のRecord読出し、callbackによるdump、clear
- 起動時に登録するプロセス単位の既定Logger
- file、line、function、module、時刻、sequenceの記録
- heap、製品domain、通信、EEPROM、systemdへの非依存
- 任意のlock/unlock callbackによるthread safetyの組込み

## 責務外

- EEPROMの製品Event/Error Log
- USB、UART、network、Cloudへの送信
- Logの圧縮、永続化、rotation
- OS固有のMutex、clock、Console deviceの生成と所有

Log UtilityはRecordを生成・蓄積・読出しするところまでを担当する。dumpしたRecordを
どこへ送るかは製品側の保守機能が決定する。

## 資料

- [初心者向け はじめてのLog Utility](README_BEGINNER.md)
- [構成と責務](docs/architecture.md)
- [API仕様](docs/api.md)
- [利用方法](docs/usage.md)
- [テスト方針](docs/quality.md)

## テスト

repository rootで実行する。

```sh
make utility-log-test
make utility-log-static-analysis
make utility-log-fuzz-smoke
make check
```
