# Repository Development Rules

## Quality Policy

- 品質基準は`tools/quality-policy.json`を唯一の定義元とする。
- 言語、Utility、moduleごとに独自の品質閾値を追加しない。
- すべての単体テストは同じ`unit_branch_min`を適用する。
- すべての結合テストは同じ`integration_branch_min`を適用する。
- 基準値を変更する場合は、特定moduleだけでなくrepository全体への影響を確認する。
- 利用者の明示的な合意なしに品質基準を変更しない。
- 新しい言語やmoduleを追加した場合、既存の総合品質レポートへ統合する。
- Coverageはrepository全体の合算だけで合否判定せず、各moduleへ共通閾値を適用する。

## C Documentation

- repository内のすべてのC headerにfile-levelのDoxygenコメントを記載する。
- すべてのC header内のmacro、型、enum値、field、関数宣言へDoxygenコメントを記載する。
- 公開APIには引数、戻り値、所有権、寿命、thread safetyを必要に応じて記載する。
- production、利用例、単体テスト、fuzz harnessをDoxygen生成対象に含める。
- 未文書化要素、引数説明不足、Doxygen構文errorを品質ゲートの失敗として扱う。
- C APIまたはC test変更時は`make c-docs-check`と`make c-docs`を実行する。

## Event Utility

- 外部公開入口は`Utility/event/include/utility_event.h`とする。
- 実装、header、単体テストは責務単位で分割する。
- Event Utility変更後は`make utility-event-test`と
  `make utility-static-analysis`を実行する。
- 完了前に`make check`を実行する。

## Log Utility

- 外部公開入口は`Utility/log/include/utility_log.h`とする。
- EEPROM製品Log、通信、USB/UART出力をApplication Log基盤へ混在させない。
- Log RecordはRAM Ring内でmessageとmetadataを所有し、呼出元pointerへ依存させない。
- 実装、header、単体テストは責務単位で分割する。
- Log Utility変更後は`make utility-log-test`と
  `make utility-log-static-analysis`を実行する。
- 完了前に`make check`を実行する。
