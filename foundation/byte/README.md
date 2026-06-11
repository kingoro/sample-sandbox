# Byte Foundation

固定長bufferを境界検査付きで読み書きするC11 Utilityである。公開入口は
`include/utility_byte.h`。heap、CRC、暗黙のnative endian変換は扱わない。

```sh
cmake -S foundation/byte -B build/foundation-byte
cmake --build build/foundation-byte
ctest --test-dir build/foundation-byte --output-on-failure
```
