# functionA

Function A固有のScenario選択、Action Queue構築、Runner操作を管理するフォルダです。

## 役割

`domain` 直下の `include/` と `src/` には、複数機能で使う共通部品を置いています。
一方で、この `functionA/` フォルダにはFunction Aだけが知っていればよい処理を置きます。

たとえば、Function AでどのScenarioを使うか、どのSequenceとStepを並べるか、
`prepare`、`start`、`pause`、`restart`、`terminate` をどう扱うかはFunction A固有の責務です。

## 構成

```text
functionA/
├── include/
│   └── function_a_manager.h
├── src/
│   └── function_a_manager.c
└── test/
    └── test_function_a_manager.c
```

`function_a_manager.h` は、Control相当から呼ばれるFunction Aの公開APIです。
`function_a_manager.c` は、Function A用のScenario、Sequence、Step定義と、
Runnerを操作する管理処理を持ちます。
`test_function_a_manager.c` は、Controlの代わりにFunction Aを呼び出す確認用プログラムです。

## 読み方

まず `src/function_a_manager.c` の上の方にあるStep配列を見てください。
そこにFunction Aで実行する最小命令が並んでいます。

次にSequence配列を見ると、Stepが手順のまとまりになっていることが分かります。
さらにScenarioを見ると、Sequenceが機能全体の流れとしてまとまっていることが分かります。

最後に `prepare_function_a()` を読むと、ScenarioがAction Queueへ展開される流れが分かります。
`start_function_a()` 以降は、実行をRunnerへ任せる流れです。
