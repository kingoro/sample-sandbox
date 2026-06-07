# ファームウェアコンポーネント

ファームウェアの共通コンポーネントを試作・検証するリポジトリ。

現在は、C ABIを持つヒープ非使用のRust製バッファ所有権ライブラリ
[`memory-buffer`](memory-buffer/README.md)を収録している。

## はじめに読む資料

1. [環境構築・開発HowTo](docs/development-guide.md)
2. [リポジトリ構成と各ファイルの責務](docs/project-structure.md)
3. [memory-bufferの使い方](memory-buffer/docs/usage.md)
4. [外部提供用C API仕様](memory-buffer/docs/api.md)

## Quick start

LinuxまたはWSL2を基準環境とする。初回だけ
[環境構築手順](docs/development-guide.md)に従って必要ツールを導入する。

```sh
# 日常のRust/Cテスト
make test

# format、静的解析、テスト、coverage、MCU buildを含む通常品質ゲート
make check

# Miriとfuzz smokeを追加したmerge前相当の検査
make extended-check
```

利用可能なMake targetは次で確認できる。

```sh
make help
```

## ドキュメント

- [環境構築・開発HowTo](docs/development-guide.md)
- [リポジトリ構成](docs/project-structure.md)
- [memory-buffer概要](memory-buffer/README.md)
- [構成・責務・状態遷移](memory-buffer/docs/architecture.md)
- [Cへの組み込み方・代表シーケンス](memory-buffer/docs/usage.md)
- [外部提供用C API仕様](memory-buffer/docs/api.md)
- [品質ゲートとHTMLレポート](memory-buffer/docs/quality.md)
- [cbindgen・Miri・fuzz・MCU向け高度検証](memory-buffer/docs/advanced-verification.md)
- [fuzz testの使い方と検査内容](fuzz/README.md)
- [全体整合性・モダン度レビュー](memory-buffer/docs/review-2026-06-07.md)

## 対応範囲

host上の単体・C ABI結合テスト、静的解析、coverage、Miri、fuzz、
`thumbv7em-none-eabi`へのcross compileまでを自動化している。実機起動、
linker script、RTOS、DMA cache制御はboard確定後に追加する範囲である。
