# Common Foundation C API・Test仕様書

この仕様書は、repository内のC公開API、production実装、C単体テスト、C fuzz
harnessからDoxygenで自動生成する。

## 対象

- Memory Buffer正式C APIと生成ABI manifest
- Memory Buffer C結合テスト兼利用例
- Event Foundation公開APIとproduction実装
- Event Foundation C単体テスト
- Event Foundation fuzz harness
- Log Foundation公開APIとproduction実装
- Log Foundation C単体テスト
- Log Foundation fuzz harness
- Byte Foundation公開API、production実装、C単体テスト
- Retry Foundation公開API、production実装、C単体テスト
- ID Foundation公開API、production実装、C単体テスト
- Thread Pool Foundation公開API、production実装、C単体テスト
- Time Foundation公開API、production実装、C単体テスト
- Linux Time Platform公開API、production実装、C単体テスト
- Time Service公開API、production実装、C単体テスト

header内のmacro、型、enum値、field、関数宣言はDoxygenコメントを必須とする。
関数契約には、引数、戻り値、所有権、寿命、thread safetyを必要に応じて記載する。
