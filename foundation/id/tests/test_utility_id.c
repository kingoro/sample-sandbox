/**
 * @file test_utility_id.c
 * @brief Request ID generator単体テスト。
 */
#include "utility_id.h"
#include "test_support.h"

#include <stdint.h>

/** @brief IDの増加、予約値、wrap、不正引数を検証する。 @return 成功時0。 */
int main(void)
{
    ut_id_generator_t generator;
    ut_id_init(&generator, 0u);
    CHECK(ut_id_next(&generator) == 1u);
    CHECK(ut_id_next(&generator) == 2u);
    ut_id_init(&generator, UINT32_MAX - 1u);
    CHECK(ut_id_next(&generator) == UINT32_MAX);
    CHECK(ut_id_next(&generator) == 1u);
    ut_id_init(NULL, 0u);
    CHECK(ut_id_next(NULL) == 0u);
    return 0;
}
