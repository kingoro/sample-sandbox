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

## Multi-Agent Workflow

- コード実装を伴う作業では、Main AgentがImplementation AgentとReview Agentを
  分けて作業を進める。
- Main Agentは要求、設計上の制約、担当fileまたはmodule、必要なtestを
  Implementation Agentへ明示する。
- Implementation Agentは割り当てられた範囲を実装し、変更file、設計判断、
  実行したtestと結果をMain Agentへ報告する。
- 各Agentは同じrepositoryで他のAgentや利用者が作業している前提とし、
  自分が作成していない変更をrevertしない。
- Implementation Agentの作業後、別のReview Agentが要求と差分を独立して確認する。
- Review Agentは原則としてfileを変更せず、bug、仕様違反、回帰risk、
  thread safety、所有権、寿命、architecture違反、test不足を確認する。
- Review指摘は重要度、file、line、理由、必要な修正を含め、重要度順に報告する。
- Main AgentはReview指摘の採否を判断し、必要な修正をImplementation Agentへ戻すか、
  自身で統合修正する。
- 指摘修正後は影響範囲のtestとrepository規則で要求された品質gateを再実行する。
- Main Agentは最終差分、Review結果、test結果を確認し、完了可否に責任を持つ。
