# Byte Utility

固定長bufferを境界検査付きで読み書きするC11 Utilityである。公開入口は
`include/utility_byte.h`。heap、CRC、暗黙のnative endian変換は扱わない。

```sh
cmake -S Utility/byte -B build/utility-byte
cmake --build build/utility-byte
ctest --test-dir build/utility-byte --output-on-failure
```
