/**
 * @file time_service.h
 * @brief UTC同期状態をmonotonic時間で進めるTime Service API。
 *
 * thread、heap、I/O、timezoneに依存しない。単一stateの同時操作はthread safeでは
 * ないため、共有時の排他は呼出側が行う。
 */
#ifndef TIME_SERVICE_H
#define TIME_SERVICE_H

#include "utility_time.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Time Service result code型。 */
typedef uint32_t time_service_result_t;

/** Time Service result code。 */
enum {
    /** 操作が成功した。 */
    TIME_SERVICE_OK = 0,
    /** NULLまたは不正snapshotが渡された。 */
    TIME_SERVICE_INVALID_ARGUMENT = 1,
    /** UTCが利用不能または未同期である。 */
    TIME_SERVICE_UNAVAILABLE = 2,
    /** 現在monotonic tickがlast syncより逆行した。 */
    TIME_SERVICE_MONOTONIC_REGRESSION = 3,
    /** UTCまたは経過時間の演算がoverflowした。 */
    TIME_SERVICE_OVERFLOW = 4
};

/** UTC同期source型。 */
typedef uint32_t time_service_source_t;

/** UTC同期source。 */
enum {
    /** source不明または未同期。 */
    TIME_SERVICE_SOURCE_UNKNOWN = 0,
    /** RTC driver由来。 */
    TIME_SERVICE_SOURCE_RTC = 1,
    /** NTP client由来。 */
    TIME_SERVICE_SOURCE_NTP = 2,
    /** PTP client由来。 */
    TIME_SERVICE_SOURCE_PTP = 3,
    /** GPS service由来。 */
    TIME_SERVICE_SOURCE_GPS = 4,
    /** 手動設定由来。 */
    TIME_SERVICE_SOURCE_MANUAL = 5
};

/** 呼出側所有のTime Service状態。 */
typedef struct time_service {
    /** last sync時点のUTC。 */
    ut_time_utc_t synchronized_utc;
    /** last sync時点のmonotonic nanosecond tick。 */
    ut_time_tick_ns_t last_sync_monotonic_ns;
    /** Last sync時点の推定不確かさnanosecond。自動増加しない。 */
    uint64_t uncertainty_ns;
    /** last syncのsource。 */
    time_service_source_t source;
    /** UTCを利用できるか。 */
    bool available;
    /** UTCが有効なsourceへ同期済みか。 */
    bool synchronized;
} time_service_t;

/** Time Service状態snapshot。 */
typedef struct time_service_status {
    /** UTCを利用できるか。 */
    bool available;
    /** UTCが有効なsourceへ同期済みか。 */
    bool synchronized;
    /** last syncのsource。 */
    time_service_source_t source;
    /** Last sync時点の推定不確かさnanosecond。自動増加しない。 */
    uint64_t uncertainty_ns;
    /** last sync時点のmonotonic tick。 */
    ut_time_tick_ns_t last_sync_monotonic_ns;
} time_service_status_t;

/**
 * Time Serviceを未同期状態へ初期化する。
 *
 * @param service 呼出側所有state。
 * @return 成功または引数不正。
 */
time_service_result_t time_service_init(time_service_t *service);

/**
 * UTC同期snapshotを更新する。
 *
 * source UNKNOWNは同期sourceとして拒否する。同期済みstateではmonotonic_nsが
 * last syncより小さいupdateをTIME_SERVICE_MONOTONIC_REGRESSIONで拒否する。
 * 値はserviceへcopyされ、入力pointerを保持しない。
 *
 * @param service 初期化済みstate。
 * @param utc sync時点のUTC。
 * @param monotonic_ns 同じ時点のmonotonic tick。
 * @param source RTC/NTP/PTP/GPS/MANUALのいずれか。
 * @param uncertainty_ns 推定不確かさ。
 * @return 成功、引数不正、またはmonotonic逆行。
 */
time_service_result_t time_service_update(
    time_service_t *service,
    const ut_time_utc_t *utc,
    ut_time_tick_ns_t monotonic_ns,
    time_service_source_t source,
    uint64_t uncertainty_ns);

/**
 * UTCを利用不能・未同期へ遷移させる。
 *
 * @param service 初期化済みstate。
 */
void time_service_set_unavailable(time_service_t *service);

/**
 * 現在monotonic tickまでlast sync UTCを進める。
 *
 * @param service 同期済みstate。
 * @param monotonic_ns 現在のmonotonic tick。
 * @param utc 呼出側所有の出力先。
 * 矛盾した内部stateはTIME_SERVICE_INVALID_ARGUMENT、正常な未同期stateは
 * TIME_SERVICE_UNAVAILABLEを返す。失敗時utcは変更しない。
 *
 * @return 成功、未同期、逆行、引数不正、またはoverflow。
 */
time_service_result_t time_service_now(
    const time_service_t *service,
    ut_time_tick_ns_t monotonic_ns,
    ut_time_utc_t *utc);

/**
 * 状態を値snapshotとして取得する。
 *
 * @param service 初期化済みstate。
 * @param status 呼出側所有の出力先。
 * 矛盾した内部stateはTIME_SERVICE_INVALID_ARGUMENTを返す。uncertaintyは
 * last sync時点の値であり、経過時間に応じて自動増加しない。
 *
 * @return 成功または引数不正。
 */
time_service_result_t time_service_get_status(
    const time_service_t *service,
    time_service_status_t *status);

#ifdef __cplusplus
}
#endif

#endif
