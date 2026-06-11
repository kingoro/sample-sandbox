/**
 * @file utility_event_trace.h
 * @brief Event履歴と状態遷移履歴を外部sinkへ通知するtrace API。
 */
#ifndef UTILITY_EVENT_TRACE_H
#define UTILITY_EVENT_TRACE_H

#include "utility_event_result.h"
#include "utility_event_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 状態IDが未指定であることを示す値。 */
#define UT_EVENT_TRACE_STATE_NONE UINT32_MAX

/** trace recordの種類。 */
typedef enum ut_event_trace_kind {
    /** Event配送またはEvent観測の履歴。 */
    UT_EVENT_TRACE_KIND_EVENT = 0,
    /** State Machine等の状態遷移履歴。 */
    UT_EVENT_TRACE_KIND_STATE_TRANSITION = 1,
    /** 有効なtrace kindの個数。閾値として使用してはならない。 */
    UT_EVENT_TRACE_KIND_COUNT = 2
} ut_event_trace_kind_t;

/**
 * sinkへ渡されるEvent・状態遷移trace record。
 *
 * recordはcallback呼出中のみ有効であり、必要な場合はsink側でcopyする。
 */
typedef struct ut_event_trace_record {
    /** trace recordの種類。 */
    ut_event_trace_kind_t kind;
    /** 対象Event ID。状態遷移のみでEventがない場合はUT_EVENT_ID_ANY。 */
    uint32_t event_id;
    /** Event発行元または状態遷移元module。未指定なら0。 */
    uint32_t source;
    /** 遷移前状態ID。Event traceではUT_EVENT_TRACE_STATE_NONE。 */
    uint32_t from_state;
    /** 遷移後状態ID。Event traceではUT_EVENT_TRACE_STATE_NONE。 */
    uint32_t to_state;
    /** 遷移理由または補助code。未指定なら0。 */
    uint32_t reason;
    /** Event payload byte数。状態遷移のみの場合は0。 */
    size_t payload_size;
} ut_event_trace_record_t;

/**
 * trace recordを受け取るsink callback。
 *
 * @param record callback呼出中のみ有効なtrace record。
 * @param user_context trace初期化時に指定した呼出側context。
 */
typedef void (*ut_event_trace_sink_t)(const ut_event_trace_record_t *record, void *user_context);

/**
 * trace sinkとcontextを保持するtrace context。
 *
 * fieldは公開しているが利用側が直接変更してはならない。内部ロックは持たず、
 * 同じcontextへの並行emitは呼出側が直列化する。
 */
typedef struct ut_event_trace {
    /** trace recordを受け取るsink callback。 */
    ut_event_trace_sink_t sink;
    /** sinkへ渡す呼出側所有context。 */
    void *user_context;
} ut_event_trace_t;

/**
 * trace contextを初期化する。
 *
 * @param trace 初期化するtrace context。
 * @param sink trace recordを受け取るcallback。
 * @param user_context sinkへそのまま渡す呼出側所有context。
 * @return UT_EVENT_OKまたはUT_EVENT_INVALID_ARGUMENT。
 */
ut_event_result_t ut_event_trace_init(ut_event_trace_t *trace, ut_event_trace_sink_t sink, void *user_context);

/**
 * Event履歴をtrace sinkへ通知する。
 *
 * payload本体はcopyせず、payload_sizeだけをrecordへ保存する。
 *
 * @param trace 初期化済みtrace context。
 * @param event 記録するEvent。
 * @return UT_EVENT_OKまたはUT_EVENT_INVALID_ARGUMENT。
 */
ut_event_result_t ut_event_trace_event(ut_event_trace_t *trace, const ut_event_t *event);

/**
 * 状態遷移履歴をtrace sinkへ通知する。
 *
 * Event FoundationのState Machineまたは呼出側独自のState Machineから使用できる。
 * このAPIは遷移結果の記録だけを担当する。
 *
 * @param trace 初期化済みtrace context。
 * @param source 遷移元moduleの識別子。未指定なら0。
 * @param from_state 遷移前状態ID。
 * @param to_state 遷移後状態ID。
 * @param reason 遷移理由または補助code。未指定なら0。
 * @return UT_EVENT_OKまたはUT_EVENT_INVALID_ARGUMENT。
 */
ut_event_result_t ut_event_trace_state_transition(ut_event_trace_t *trace, uint32_t source, uint32_t from_state, uint32_t to_state, uint32_t reason);

/**
 * DispatcherのUT_EVENT_ID_ANY購読に登録できるEvent trace handler。
 *
 * user_contextには初期化済みut_event_trace_tへのpointerを指定する。
 *
 * @param event 配送されたEvent。
 * @param user_context ut_event_trace_tへのpointer。
 */
void ut_event_trace_dispatch_handler(const ut_event_t *event, void *user_context);

#ifdef __cplusplus
}
#endif

#endif
