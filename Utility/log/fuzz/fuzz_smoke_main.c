/**
 * @file fuzz_smoke_main.c
 * @brief Log Utility fuzz harnessを決定的入力で実行するprogram。
 */
#include <stddef.h>
#include <stdint.h>

/** @cond INTERNAL */
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);
/** @endcond */

/**
 * 2000個の決定的な操作列をfuzz入口へ渡す。
 *
 * @return 全入力を処理した場合0。
 */
int main(void)
{
    uint8_t data[256];
    uint32_t state = 0x4C4F4721u;
    size_t run;

    for (run = 0u; run < 2000u; run++) {
        size_t index;
        const size_t size = run % sizeof(data);
        for (index = 0u; index < size; index++) {
            state = state * 1664525u + 1013904223u;
            data[index] = (uint8_t)(state >> 24u);
        }
        (void)LLVMFuzzerTestOneInput(data, size);
    }
    return 0;
}
