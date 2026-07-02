#include "domain_status_dispatcher.h"

#include <stdio.h>

/**
 * @file domain_status_dispatcher.c
 * @brief domain層の状態変化を通知するDispatcherの実装。
 *
 * Dispatcherは、domain層の状態変化をControl相当へ渡すための小さな部品です。
 * このサンプルではコールバック関数を1つ登録し、状態変化のたびに呼び出します。
 */

/**
 * @copydoc initialize_domain_status_dispatcher
 */
bool initialize_domain_status_dispatcher(
    domain_status_dispatcher_t *dispatcher,
    domain_status_handler_fn handler,
    void *user_context)
{
    if (dispatcher == NULL) {
        return false;
    }

    dispatcher->handler = handler;
    dispatcher->user_context = user_context;
    dispatcher->function_name = NULL;
    return true;
}

bool set_domain_status_dispatcher_function_name(
    domain_status_dispatcher_t *dispatcher,
    const char *function_name)
{
    if (dispatcher == NULL) {
        return false;
    }

    dispatcher->function_name = function_name;
    return true;
}

/**
 * @copydoc dispatch_domain_status
 *
 * handlerがNULLの場合でもエラーにはしません。
 * ログだけ出して処理を続けられるようにしています。
 */
bool dispatch_domain_status(
    const domain_status_dispatcher_t *dispatcher,
    domain_status_event_t event,
    const char *message)
{
    domain_status_report_t report;

    report.event = event;
    report.function_name = NULL;
    report.scenario_name = NULL;
    report.sequence_name = NULL;
    report.step_name = NULL;
    report.action_name = NULL;
    report.step_index = 0U;
    report.total_step_count = 0U;
    report.error_code = 0;
    report.message = message;

    return dispatch_domain_status_report(dispatcher, &report);
}

bool dispatch_domain_status_report(
    const domain_status_dispatcher_t *dispatcher,
    const domain_status_report_t *report)
{
    if (dispatcher == NULL) {
        return false;
    }

    if (report == NULL) {
        return false;
    }

    printf("[domain] dispatch_domain_status(event=%s, function=%s, scenario=%s, sequence=%s, step=%s, progress=%zu/%zu, error=%d, message=%s)\n",
           convert_domain_status_event_to_string(report->event),
           (report->function_name != NULL) ? report->function_name :
                                             ((dispatcher->function_name != NULL) ? dispatcher->function_name : ""),
           (report->scenario_name != NULL) ? report->scenario_name : "",
           (report->sequence_name != NULL) ? report->sequence_name : "",
           (report->step_name != NULL) ? report->step_name : "",
           report->step_index,
           report->total_step_count,
           report->error_code,
           (report->message != NULL) ? report->message : "");

    if (dispatcher->handler != NULL) {
        domain_status_report_t completed_report = *report;

        if (completed_report.function_name == NULL) {
            completed_report.function_name = dispatcher->function_name;
        }

        /*
         * Control相当へ状態イベントを通知します。
         * user_contextは呼び出し元が自由に使える領域です。
         */
        dispatcher->handler(&completed_report, dispatcher->user_context);
    }

    return true;
}

/**
 * @copydoc convert_domain_status_event_to_string
 */
const char *convert_domain_status_event_to_string(domain_status_event_t event)
{
    switch (event) {
    case DOMAIN_STATUS_EVENT_PREPARED:
        return "PREPARED";
    case DOMAIN_STATUS_EVENT_STARTED:
        return "STARTED";
    case DOMAIN_STATUS_EVENT_ACTION_COMPLETED:
        return "ACTION_COMPLETED";
    case DOMAIN_STATUS_EVENT_PAUSED:
        return "PAUSED";
    case DOMAIN_STATUS_EVENT_RESTARTED:
        return "RESTARTED";
    case DOMAIN_STATUS_EVENT_COMPLETED:
        return "COMPLETED";
    case DOMAIN_STATUS_EVENT_TERMINATED:
        return "TERMINATED";
    case DOMAIN_STATUS_EVENT_ERROR:
        return "ERROR";
    default:
        return "UNKNOWN";
    }
}
