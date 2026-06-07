# テスト・品質方針

単体テストでは次を検証する。

- ConsoleとRAM Ringの独立level filter
- 実行時の有効・無効切替
- 無効Log macroでformat引数を評価しないこと
- messageとmetadataがRecordへcopyされること
- Ring満杯時の上書き、順序、上書き件数
- read、dump中断、clear
- clock、Console、lock/unlock callback
- 不正引数と壊れたcontextの拒否
- FILE Console adapterの出力形式
- 任意操作列に対するRing状態不変条件とsanitizer検査

品質基準はrepository共通の`tools/quality-policy.json`に従う。Log Utilityだけの
独自閾値は設けない。Doxygenコメント検査、GCC静的解析、C coverage、metrics、
総合品質reportへ統合する。C branch coverageはC Utility全体の合算ではなく、
`Utility/log`単位で共通の単体テスト基準を満たさなければならない。
