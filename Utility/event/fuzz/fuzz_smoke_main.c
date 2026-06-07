/**
 * @file fuzz_smoke_main.c
 * @brief libFuzzerなしでfuzz targetを短時間実行するdriver。
 */
#include <stddef.h>
#include <stdint.h>

/** @cond INTERNAL */
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);
/** @endcond */

/**
 * xorshift32で次の疑似乱数を生成する。
 *
 * @param state 更新対象の乱数状態。
 * @return 次の32 bit疑似乱数。
 */
static uint32_t next_random(uint32_t *state)
{
    uint32_t value = *state;

    value ^= value << 13u;
    value ^= value >> 17u;
    value ^= value << 5u;
    *state = value;
    return value;
}

/**
 * 2,000件の疑似ランダム入力でfuzz targetを実行する。
 *
 * @return 全入力成功時は0。
 */
int main(void)
{
    uint8_t input[256];
    uint32_t random_state = 0x91e10da5u;
    size_t run;
    size_t index;

    for (run = 0u; run < 2000u; run++) {
        const size_t length = (next_random(&random_state) % sizeof(input)) + 1u;

        for (index = 0u; index < length; index++) {
            input[index] = (uint8_t)next_random(&random_state);
        }
        (void)LLVMFuzzerTestOneInput(input, length);
    }

    return 0;
}
