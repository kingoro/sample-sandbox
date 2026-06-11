/**
 * @file utility_event_state_machine.h
 * @brief Event駆動のtable-driven State Machine公開API。
 */
#ifndef UTILITY_EVENT_STATE_MACHINE_H
#define UTILITY_EVENT_STATE_MACHINE_H

#include "utility_event_result.h"
#include "utility_event_trace.h"
#include "utility_event_types.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** すべての状態IDに一致する遷移定義用ID。通常状態IDとしては使用しない。 */
#define UT_EVENT_STATE_ID_ANY UINT32_MAX

/** State Machineの遷移option bit。 */
typedef uint32_t ut_event_state_transition_options_t;

/** 遷移時のexit、entry callbackを呼ばない内部遷移。 */
#define UT_EVENT_STATE_TRANSITION_INTERNAL 0x01u

/** State Machine contextの前方宣言。 */
typedef struct ut_event_state_machine ut_event_state_machine_t;

/**
 * 状態遷移を許可するか判定するguard callback。
 *
 * @param machine 実行中のState Machine。
 * @param event 入力Event。
 * @param user_context 遷移定義で指定した呼出側context。
 * @return 遷移を許可する場合true、拒否する場合false。
 */
typedef bool (*ut_event_state_guard_t)(const ut_event_state_machine_t *machine, const ut_event_t *event, void *user_context);

/**
 * 状態のentryまたはexit時に呼ばれるcallback。
 *
 * @param machine 実行中のState Machine。
 * @param event 遷移を発生させたEvent。
 * @param user_context 状態定義で指定した呼出側context。
 */
typedef void (*ut_event_state_hook_t)(ut_event_state_machine_t *machine, const ut_event_t *event, void *user_context);

/**
 * 遷移時に呼ばれるaction callback。
 *
 * @param machine 実行中のState Machine。
 * @param event 遷移を発生させたEvent。
 * @param user_context 遷移定義で指定した呼出側context。
 */
typedef void (*ut_event_state_action_t)(ut_event_state_machine_t *machine, const ut_event_t *event, void *user_context);

/** State Machineで使用できる1つの状態定義。 */
typedef struct ut_event_state {
    /** applicationが定義する状態ID。UT_EVENT_STATE_ID_ANYは使用できない。 */
    uint32_t id;
    /** 診断やtable review用の状態名。NULLでもよい。 */
    const char *name;
    /** 状態へ入るときに呼ぶcallback。不要ならNULL。 */
    ut_event_state_hook_t entry;
    /** 状態から出るときに呼ぶcallback。不要ならNULL。 */
    ut_event_state_hook_t exit;
    /** entryとexitへ渡す呼出側所有context。 */
    void *user_context;
} ut_event_state_t;

/** State Machineで使用できる1つの遷移定義。 */
typedef struct ut_event_state_transition {
    /** 遷移元状態ID。UT_EVENT_STATE_ID_ANYで全状態に一致する。 */
    uint32_t from_state;
    /** 入力Event ID。UT_EVENT_ID_ANYで全Eventに一致する。 */
    uint32_t event_id;
    /** 遷移先状態ID。内部遷移でも有効な状態IDを指定する。 */
    uint32_t to_state;
    /** 遷移可否を判定するguard。不要ならNULL。 */
    ut_event_state_guard_t guard;
    /** 遷移時に実行するaction。不要ならNULL。 */
    ut_event_state_action_t action;
    /** guardとactionへ渡す呼出側所有context。 */
    void *user_context;
    /** UT_EVENT_STATE_TRANSITION_* のbit集合。 */
    ut_event_state_transition_options_t options;
    /** traceへ出す遷移理由code。不要なら0。 */
    uint32_t reason;
} ut_event_state_transition_t;

/**
 * table-driven State Machine context。
 *
 * fieldは公開しているが利用側が直接変更してはならない。heap、thread、I/Oには依存せず、
 * 同じcontextへの並行操作は呼出側が直列化する。
 */
struct ut_event_state_machine {
    /** 状態定義table。 */
    const ut_event_state_t *states;
    /** 状態定義数。 */
    size_t state_count;
    /** 遷移定義table。 */
    const ut_event_state_transition_t *transitions;
    /** 遷移定義数。 */
    size_t transition_count;
    /** 現在状態ID。 */
    uint32_t current_state;
    /** applicationまたはmoduleの識別子。trace sourceへ使用する。 */
    uint32_t source;
    /** callbackから参照できる呼出側所有context。 */
    void *user_context;
    /** 任意のtrace context。NULLならtraceしない。 */
    ut_event_trace_t *trace;
    /** dispatch中なら1。再入を拒否するために使用する。 */
    uint8_t dispatching;
};

/**
 * State Machineを初期化する。
 *
 * states、transitions、user_contextは再初期化または利用終了まで有効に保つ。
 *
 * @param machine 初期化するState Machine context。
 * @param states 状態定義table。
 * @param state_count 状態定義数。
 * @param transitions 遷移定義table。
 * @param transition_count 遷移定義数。
 * @param initial_state 初期状態ID。
 * @param source trace sourceへ使用するmodule識別子。
 * @param user_context callbackから参照できる呼出側所有context。
 * @return UT_EVENT_OKまたはUT_EVENT_INVALID_ARGUMENT。
 */
ut_event_result_t ut_event_state_machine_init(ut_event_state_machine_t *machine, const ut_event_state_t *states, size_t state_count, const ut_event_state_transition_t *transitions, size_t transition_count, uint32_t initial_state, uint32_t source, void *user_context);

/**
 * State Machineへ任意のtrace contextを接続する。
 *
 * @param machine 初期化済みState Machine context。
 * @param trace trace context。NULLを指定するとtraceを無効化する。
 * @return UT_EVENT_OKまたはUT_EVENT_INVALID_ARGUMENT。
 */
ut_event_result_t ut_event_state_machine_set_trace(ut_event_state_machine_t *machine, ut_event_trace_t *trace);

/**
 * EventをState Machineへ入力し、最初に一致した遷移を実行する。
 *
 * 遷移tableは登録順に評価する。from_stateとevent_idが一致し、guardがNULLまたはtrueを
 * 返した最初の遷移だけを実行する。該当遷移がない場合はUT_EVENT_NOT_FOUNDを返す。
 *
 * @param machine 初期化済みState Machine context。
 * @param event 入力Event。
 * @param out_new_state 遷移後状態IDの任意の格納先。不要ならNULL。
 * @return UT_EVENT_OK、UT_EVENT_NOT_FOUND、UT_EVENT_BUSY、
 * UT_EVENT_INVALID_ARGUMENT。
 */
ut_event_result_t ut_event_state_machine_dispatch(ut_event_state_machine_t *machine, const ut_event_t *event, uint32_t *out_new_state);

/**
 * State Machineの現在状態IDを返す。
 *
 * @param machine State Machine context。
 * @return 現在状態ID。無効なcontextではUT_EVENT_STATE_ID_ANY。
 */
uint32_t ut_event_state_machine_current_state(const ut_event_state_machine_t *machine);

/**
 * State Machine初期化時に指定した呼出側contextを返す。
 *
 * @param machine State Machine context。
 * @return user_context。無効なcontextではNULL。
 */
void *ut_event_state_machine_context(const ut_event_state_machine_t *machine);

#ifdef __cplusplus
}
#endif

#endif
