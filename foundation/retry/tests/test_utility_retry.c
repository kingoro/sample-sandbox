/**
 * @file test_utility_retry.c
 * @brief Retry policy/state単体テスト。
 */
#include "utility_retry.h"
#include "test_support.h"

/** @brief 固定delayと試行上限を検証する。 @return 成功時0。 */
static int test_fixed_and_reset(void)
{
    const ut_retry_policy_t policy = {3u, 7u, 20u, UT_RETRY_BACKOFF_FIXED};
    ut_retry_state_t state;
    uint64_t delay = 0u;
    CHECK(ut_retry_init(&state, &policy) == UT_RETRY_OK);
    CHECK(ut_retry_can_attempt(&state));
    CHECK(ut_retry_begin_attempt(&state) == UT_RETRY_OK);
    CHECK(ut_retry_failure(&state, &delay) == UT_RETRY_OK && delay == 7u);
    CHECK(ut_retry_begin_attempt(&state) == UT_RETRY_OK);
    CHECK(ut_retry_failure(&state, &delay) == UT_RETRY_OK && delay == 7u);
    CHECK(ut_retry_begin_attempt(&state) == UT_RETRY_OK);
    CHECK(!ut_retry_can_attempt(&state));
    CHECK(ut_retry_failure(&state, &delay) == UT_RETRY_EXHAUSTED);
    CHECK(ut_retry_begin_attempt(&state) == UT_RETRY_EXHAUSTED);
    ut_retry_success(&state);
    CHECK(state.attempts == 0u);
    state.attempts = 2u;
    ut_retry_reset(&state);
    CHECK(state.attempts == 0u);
    return 0;
}

/** @brief 指数backoff、clamp、overflow回避を検証する。 @return 成功時0。 */
static int test_exponential_clamp(void)
{
    const ut_retry_policy_t policy = {
        6u, UINT64_C(0x4000000000000000), UINT64_MAX,
        UT_RETRY_BACKOFF_EXPONENTIAL
    };
    ut_retry_state_t state;
    uint64_t delay;
    CHECK(ut_retry_init(&state, &policy) == UT_RETRY_OK);
    CHECK(ut_retry_begin_attempt(&state) == UT_RETRY_OK);
    CHECK(ut_retry_failure(&state, &delay) == UT_RETRY_OK);
    CHECK(delay == UINT64_C(0x4000000000000000));
    CHECK(ut_retry_begin_attempt(&state) == UT_RETRY_OK);
    CHECK(ut_retry_failure(&state, &delay) == UT_RETRY_OK);
    CHECK(delay == UINT64_C(0x8000000000000000));
    CHECK(ut_retry_begin_attempt(&state) == UT_RETRY_OK);
    CHECK(ut_retry_failure(&state, &delay) == UT_RETRY_OK);
    CHECK(delay == UINT64_MAX);
    return 0;
}

/** @brief 不正引数を検証する。 @return 成功時0。 */
static int test_invalid_arguments(void)
{
    ut_retry_policy_t policy = {1u, 0u, 0u, UT_RETRY_BACKOFF_FIXED};
    ut_retry_state_t state;
    uint64_t delay;
    CHECK(ut_retry_init(NULL, &policy) == UT_RETRY_INVALID_ARGUMENT);
    CHECK(ut_retry_init(&state, NULL) == UT_RETRY_INVALID_ARGUMENT);
    policy.max_attempts = 0u;
    CHECK(ut_retry_init(&state, &policy) == UT_RETRY_INVALID_ARGUMENT);
    policy.max_attempts = 1u;
    policy.base_delay = 2u;
    policy.max_delay = 1u;
    CHECK(ut_retry_init(&state, &policy) == UT_RETRY_INVALID_ARGUMENT);
    policy.base_delay = 0u;
    policy.max_delay = 0u;
    policy.backoff = 99u;
    CHECK(ut_retry_init(&state, &policy) == UT_RETRY_INVALID_ARGUMENT);
    policy.backoff = UT_RETRY_BACKOFF_FIXED;
    CHECK(ut_retry_init(&state, &policy) == UT_RETRY_OK);
    CHECK(ut_retry_failure(&state, &delay) == UT_RETRY_INVALID_ARGUMENT);
    CHECK(ut_retry_failure(&state, NULL) == UT_RETRY_INVALID_ARGUMENT);
    CHECK(!ut_retry_can_attempt(NULL));
    CHECK(ut_retry_begin_attempt(NULL) == UT_RETRY_INVALID_ARGUMENT);
    ut_retry_reset(NULL);
    ut_retry_success(NULL);
    return 0;
}

/** @brief Retry Foundation単体テストを実行する。 @return 全成功時0。 */
int main(void)
{
    CHECK(test_fixed_and_reset() == 0);
    CHECK(test_exponential_clamp() == 0);
    CHECK(test_invalid_arguments() == 0);
    return 0;
}
