#ifndef DOMAIN_STATUS_DISPATCHER_H
#define DOMAIN_STATUS_DISPATCHER_H

#include <stdbool.h>
#include <stddef.h>

/**
 * @file domain_status_dispatcher.h
 * @brief domain層の状態変化を外側へ通知する仕組みを定義する。
 */

/**
 * @brief domain層から通知するイベント種別。
 */
typedef enum {
    DOMAIN_STATUS_EVENT_PREPARED = 0,
    DOMAIN_STATUS_EVENT_STARTED,
    DOMAIN_STATUS_EVENT_ACTION_COMPLETED,
    DOMAIN_STATUS_EVENT_PAUSED,
    DOMAIN_STATUS_EVENT_RESTARTED,
    DOMAIN_STATUS_EVENT_COMPLETED,
    DOMAIN_STATUS_EVENT_TERMINATED,
    DOMAIN_STATUS_EVENT_ERROR
} domain_status_event_t;

/**
 * @brief domain層からControl相当へ通知する状態レポート。
 *
 * 進捗表示や異常時の掃除処理に必要な情報を、文字列messageだけでなく構造化して渡します。
 */
typedef struct {
    /** 通知イベント。 */
    domain_status_event_t event;
    /** 機能名。例: "Function A"。 */
    const char *function_name;
    /** Scenario名。 */
    const char *scenario_name;
    /** Sequence名。 */
    const char *sequence_name;
    /** Step名。 */
    const char *step_name;
    /** Action名。 */
    const char *action_name;
    /** 1始まりのStep番号。未設定の場合は0。 */
    size_t step_index;
    /** Scenario全体のStep数。未設定の場合は0。 */
    size_t total_step_count;
    /** エラーコード。正常系は0。 */
    int error_code;
    /** 補足メッセージ。 */
    const char *message;
} domain_status_report_t;

/**
 * @brief 状態通知を受け取るコールバック関数。
 *
 * @param report 状態レポート。
 * @param user_context 呼び出し元が登録した任意の情報。
 */
typedef void (*domain_status_handler_fn)(
    const domain_status_report_t *report,
    void *user_context);

/**
 * @brief 状態通知先を保持するDispatcher。
 */
typedef struct {
    domain_status_handler_fn handler;
    void *user_context;
    /** 通知元の機能名。例: "Function A"。 */
    const char *function_name;
} domain_status_dispatcher_t;

/**
 * @brief Dispatcherに機能名を設定する。
 *
 * @param dispatcher 対象Dispatcher。
 * @param function_name 通知元として表示する機能名。
 * @return 設定できた場合は `true`、引数が不正な場合は `false`。
 */
bool set_domain_status_dispatcher_function_name(
    domain_status_dispatcher_t *dispatcher,
    const char *function_name);

/**
 * @brief Dispatcherを初期化する。
 *
 * @param dispatcher 初期化するDispatcher。
 * @param handler 通知先コールバック。未使用の場合は `NULL`。
 * @param user_context コールバックへ渡す任意の情報。
 * @return 初期化できた場合は `true`、引数が不正な場合は `false`。
 */
bool initialize_domain_status_dispatcher(
    domain_status_dispatcher_t *dispatcher,
    domain_status_handler_fn handler,
    void *user_context);

/**
 * @brief 状態イベントを通知する。
 *
 * @param dispatcher 通知に使うDispatcher。
 * @param event 通知イベント。
 * @param message 補足メッセージ。詳細文脈が必要な場合は `dispatch_domain_status_report()` を使います。
 * @return 通知処理を実行できた場合は `true`、引数が不正な場合は `false`。
 */
bool dispatch_domain_status(
    const domain_status_dispatcher_t *dispatcher,
    domain_status_event_t event,
    const char *message);

/**
 * @brief 状態レポートを通知する。
 *
 * @param dispatcher 通知に使うDispatcher。
 * @param report 通知する状態レポート。
 * @return 通知処理を実行できた場合は `true`、引数が不正な場合は `false`。
 */
bool dispatch_domain_status_report(
    const domain_status_dispatcher_t *dispatcher,
    const domain_status_report_t *report);

/**
 * @brief 状態イベントを表示用文字列へ変換する。
 *
 * @param event 変換するイベント。
 * @return イベント名を表す静的文字列。
 */
const char *convert_domain_status_event_to_string(domain_status_event_t event);

#endif
