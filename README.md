# Rust Utilities

組み込み用途を中心としたRust utilityを管理するCargo workspace。

## Crates

- [`memory-buffer`](memory-buffer/README.md): ヒープ非使用でC ABI互換の
  memory buffer所有権library

新しいutilityはworkspace memberとして追加し、共通の品質基準は
`tools/quality-policy.json`で管理する。

## Verification

```sh
make test
make check
```

Miriとfuzz smokeを含む検査:

```sh
make extended-check
```

