# 構成と責務

## コンポーネント境界

`memory-buffer`は末端の共通コンポーネントである。byte列と所有権だけを扱い、
そのデータが印刷用なのか、通信受信用なのかは認識しない。

```mermaid
flowchart TB
    Control["Control / Domain Service (C)"]
    DataPlane["Data Plane (CまたはRust)"]
    Buffer["memory-buffer (Rust no_std)"]
    Driver["Driver / Internal Interface (C)"]
    Unit["Unit / Sub基盤"]
    RAM[("呼出側が提供するRAM arena")]

    Control -->|"commandとhandle"| DataPlane
    DataPlane -->|"alloc / read / write / free"| Buffer
    Buffer -->|"領域metadataを所有"| RAM
    DataPlane -->|"DMA用map貸出"| Driver
    Driver --> Unit

    classDef rust fill:#f5d0a9,stroke:#9c4f00,color:#222;
    classDef c fill:#cfe8ff,stroke:#245b8a,color:#222;
    class Buffer rust;
    class Control,DataPlane,Driver c;
```

Data Planeはpayloadの業務上の生存期間を所有する。`memory-buffer`はRAM領域の
生存期間だけを所有する。Driverにはバッファ所有権ではなく一時ポインタを貸し出す。

## 内部構成

```mermaid
flowchart LR
    C["C ABI"]
    Validation["引数・境界検査"]
    Handles["世代付きhandle table"]
    Allocator["first-fit領域allocator"]
    Arena[("外部byte arena")]

    C --> Validation
    Validation --> Handles
    Handles --> Allocator
    Allocator --> Arena
```

1つの`mb_context_t`は最大64個の生存中バッファmetadataを保持する。payload本体は
context内には格納せず、`mb_init`へ渡されたarenaに置く。

```mermaid
block-beta
    columns 1
    Context["mb_context_t: magic、arena pointer、64個のslot"]
    Arena["arena: buffer A | 空き | buffer C | 空き"]
```

各slotが保持する情報は次のとおり。

- arena内offset
- 論理長
- 割当capacity
- 世代番号
- map貸出数

C handleにはslot indexと世代番号を符号化する。slot解放時に世代を進めるため、
古いhandleから同じslotに後で作られた別バッファへアクセスできない。

## バッファ状態遷移

```mermaid
stateDiagram-v2
    [*] --> Free
    Free --> Allocated: mb_alloc
    Allocated --> Allocated: mb_read / mb_write / mb_set_length
    Allocated --> Mapped: mb_map
    Mapped --> Mapped: read/write/free/mapはMB_BUSY
    Mapped --> Allocated: mb_unmap
    Allocated --> Free: mb_free
    Allocated --> Free: mb_reset
```

mapは排他的貸出であり、二重`mb_map`を拒否する。map中は`mb_read`、`mb_write`、
`mb_set_length`、`mb_free`、`mb_reset`が`MB_BUSY`を返す。これによりDMAとCPU、
または複数taskによる同一領域への競合accessをlibrary境界で防ぐ。

## 制約

- context内部では排他制御しない
- 生存中データのcompactionは行わない
- DMA cache maintenanceとarena base以上のalignment調整はplatform側で行う
- mapしたポインタを対応する`mb_unmap`後まで保持してはならない
- 再初期化前に、以前の利用者をすべて停止しなければならない
