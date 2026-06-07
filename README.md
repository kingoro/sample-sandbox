# ファームウェアコンポーネント

ファームウェアの共通コンポーネントを試作・検証するリポジトリ。

- [`memory-buffer`](memory-buffer/README.md): C ABIを持つ、ヒープ非使用の
  Rust製バッファ所有権ライブラリ
  - [構成・責務・状態遷移](memory-buffer/docs/architecture.md)
  - [使い方・代表シーケンス](memory-buffer/docs/usage.md)
  - [外部提供用C API仕様](memory-buffer/docs/api.md)
  - [品質ゲートとHTMLレポート](memory-buffer/docs/quality.md)
  - [cbindgen・Miri・fuzz・MCU向け高度検証](memory-buffer/docs/advanced-verification.md)
  - [全体整合性・モダン度レビュー](memory-buffer/docs/review-2026-06-07.md)
