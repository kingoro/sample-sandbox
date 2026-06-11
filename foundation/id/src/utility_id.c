/**
 * @file utility_id.c
 * @brief uint32 request ID generator実装。
 */
#include "utility_id.h"

#include <stddef.h>
#include <stdint.h>

void ut_id_init(ut_id_generator_t *generator, uint32_t current)
{
    if (generator != NULL) {
        generator->current = current;
    }
}

uint32_t ut_id_next(ut_id_generator_t *generator)
{
    if (generator == NULL) {
        return 0u;
    }
    if (generator->current == UINT32_MAX) {
        generator->current = 1u;
    } else {
        generator->current++;
        if (generator->current == 0u) {
            generator->current = 1u;
        }
    }
    return generator->current;
}
