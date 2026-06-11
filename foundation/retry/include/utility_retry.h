/**
 * @file utility_retry.h
 * @brief Clockやsleepに依存しないretry policy/state API。
 */
#ifndef UTILITY_RETRY_H
#define UTILITY_RETRY_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Retry APIのresult code型。 */
typedef uint32_t ut_retry_result_t;

/** Retry APIのresult code。 */
enum {
    /** 操作が成功した。 */
    UT_RETRY_OK = 0,
    /** NULLまたは不正なpolicy/stateが渡された。 */
    UT_RETRY_INVALID_ARGUMENT = 1,
    /** 最大試行回数へ到達している。 */
    UT_RETRY_EXHAUSTED = 2
};

/** Delay計算方式の型。 */
typedef uint32_t ut_retry_backoff_t;

/** Delay計算方式。 */
enum {
    /** 毎回base_delayを返す。 */
    UT_RETRY_BACKOFF_FIXED = 0,
    /** Retryごとにbase_delayを2倍しmax_delayでclampする。 */
    UT_RETRY_BACKOFF_EXPONENTIAL = 1
};

/** Retry回数とdelayを定義するpolicy。 */
typedef struct ut_retry_policy {
    /** 初回を含む最大試行回数。1以上。 */
    uint32_t max_attempts;
    /** Retry前の基準delay。単位は呼出側が決める。 */
    uint64_t base_delay;
    /** Delay上限。base_delay以上。 */
    uint64_t max_delay;
    /** 固定または指数backoff。 */
    ut_retry_backoff_t backoff;
} ut_retry_policy_t;

/**
 * 呼出側が所有するretry状態。
 *
 * heap、thread、clockを使わない。単一contextの同時操作はthread safeではない。
 */
typedef struct ut_retry_state {
    /** 適用中のpolicy copy。 */
    ut_retry_policy_t policy;
    /** 現在のoperationで開始済みの試行回数。 */
    uint32_t attempts;
} ut_retry_state_t;

/**
 * Retry状態を初期化する。
 *
 * @param state 初期化する呼出側所有state。
 * @param policy copyするpolicy。
 * @return UT_RETRY_OKまたはUT_RETRY_INVALID_ARGUMENT。
 */
ut_retry_result_t ut_retry_init(
    ut_retry_state_t *state,
    const ut_retry_policy_t *policy);

/**
 * 次の試行を開始してattempt数を増やす。
 *
 * @param state 初期化済みstate。
 * @return 開始可能ならUT_RETRY_OK、上限ならUT_RETRY_EXHAUSTED。
 */
ut_retry_result_t ut_retry_begin_attempt(ut_retry_state_t *state);

/**
 * 失敗後に次回retryが可能か判定し、その前に待つdelayを計算する。
 *
 * 実際のsleepやI/Oは呼出側の責任である。overflowする乗算は行わずmax_delayへ
 * clampする。
 *
 * @param state 1回以上begin済みのstate。
 * @param delay 次回retry前delayの出力先。
 * @return 次回があればUT_RETRY_OK、上限ならUT_RETRY_EXHAUSTED。
 */
ut_retry_result_t ut_retry_failure(
    const ut_retry_state_t *state,
    uint64_t *delay);

/**
 * 次の試行を開始できるか返す。
 *
 * @param state 初期化済みstate。
 * @return attemptsがmax_attempts未満ならtrue。
 */
bool ut_retry_can_attempt(const ut_retry_state_t *state);

/**
 * 成功として状態を初期値へ戻す。
 *
 * @param state 初期化済みstate。
 */
void ut_retry_success(ut_retry_state_t *state);

/**
 * Retry状態を初期値へ戻す。
 *
 * @param state 初期化済みstate。
 */
void ut_retry_reset(ut_retry_state_t *state);

#ifdef __cplusplus
}
#endif

#endif
