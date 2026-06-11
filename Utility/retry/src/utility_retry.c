/**
 * @file utility_retry.c
 * @brief Retry policy/state実装。
 */
#include "utility_retry.h"

#include <stddef.h>

/**
 * Policyのfield組合せが有効か確認する。
 *
 * @param policy 確認するpolicy。
 * @return 有効ならtrue。
 */
static bool valid_policy(const ut_retry_policy_t *policy)
{
    return policy != NULL &&
        policy->max_attempts != 0u &&
        policy->base_delay <= policy->max_delay &&
        (policy->backoff == UT_RETRY_BACKOFF_FIXED ||
         policy->backoff == UT_RETRY_BACKOFF_EXPONENTIAL);
}

ut_retry_result_t ut_retry_init(
    ut_retry_state_t *state,
    const ut_retry_policy_t *policy)
{
    if (state == NULL || !valid_policy(policy)) {
        return UT_RETRY_INVALID_ARGUMENT;
    }
    state->policy = *policy;
    state->attempts = 0u;
    return UT_RETRY_OK;
}

bool ut_retry_can_attempt(const ut_retry_state_t *state)
{
    return state != NULL && valid_policy(&state->policy) &&
        state->attempts < state->policy.max_attempts;
}

ut_retry_result_t ut_retry_begin_attempt(ut_retry_state_t *state)
{
    if (state == NULL || !valid_policy(&state->policy)) {
        return UT_RETRY_INVALID_ARGUMENT;
    }
    if (!ut_retry_can_attempt(state)) {
        return UT_RETRY_EXHAUSTED;
    }
    state->attempts++;
    return UT_RETRY_OK;
}

ut_retry_result_t ut_retry_failure(
    const ut_retry_state_t *state,
    uint64_t *delay)
{
    uint64_t value;
    uint32_t shifts;
    if (state == NULL || delay == NULL || !valid_policy(&state->policy) ||
        state->attempts == 0u) {
        return UT_RETRY_INVALID_ARGUMENT;
    }
    if (!ut_retry_can_attempt(state)) {
        return UT_RETRY_EXHAUSTED;
    }
    value = state->policy.base_delay;
    shifts = state->attempts - 1u;
    if (state->policy.backoff == UT_RETRY_BACKOFF_EXPONENTIAL &&
        value != 0u) {
        while (shifts != 0u && value < state->policy.max_delay) {
            if (value > state->policy.max_delay / 2u) {
                value = state->policy.max_delay;
                break;
            }
            value *= 2u;
            shifts--;
        }
    }
    *delay = value;
    return UT_RETRY_OK;
}

void ut_retry_reset(ut_retry_state_t *state)
{
    if (state != NULL) {
        state->attempts = 0u;
    }
}

void ut_retry_success(ut_retry_state_t *state)
{
    ut_retry_reset(state);
}
