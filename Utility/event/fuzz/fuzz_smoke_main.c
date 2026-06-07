#include <stddef.h>
#include <stdint.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

static uint32_t next_random(uint32_t *state)
{
    uint32_t value = *state;

    value ^= value << 13u;
    value ^= value >> 17u;
    value ^= value << 5u;
    *state = value;
    return value;
}

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
