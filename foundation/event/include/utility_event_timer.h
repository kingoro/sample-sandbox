/**
 * @file utility_event_timer.h
 * @brief 外部clockのtickでEventを発行するTimer Scheduler公開API。
 */
#ifndef UTILITY_EVENT_TIMER_H
#define UTILITY_EVENT_TIMER_H

#include "utility_event_queue.h"
#include "utility_event_result.h"
#include "utility_event_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** applicationがTimerを識別するID型。 */
typedef uint32_t ut_event_timer_id_t;

/** Timerが使用する1件の固定長slot。 */
typedef struct ut_event_timer_slot {
    /** applicationが指定したTimer ID。 */
    ut_event_timer_id_t id;
    /** deadline到達時にQueueへcopyするEvent記述子。 */
    ut_event_t event;
    /** 次回発火時刻を表す単調増加tick。 */
    uint64_t deadline;
    /** 周期tick。0ならone-shot。 */
    uint64_t period;
    /** slotが使用中なら1、未使用なら0。 */
    uint8_t active;
} ut_event_timer_slot_t;

/**
 * 呼出側提供storageを管理するTimer Scheduler。
 *
 * fieldは公開しているが利用側が直接変更してはならない。clock、thread、RTOS、I/Oを
 * 所有せず、呼出側が渡す単調増加tickだけで動作する。
 */
typedef struct ut_event_timer_scheduler {
    /** 呼出側が提供するTimer slot配列。 */
    ut_event_timer_slot_t *slots;
    /** slot配列の件数。 */
    size_t capacity;
    /** activeなTimer件数。 */
    size_t count;
    /** process中なら1。再入と設定変更を拒否する。 */
    uint8_t processing;
} ut_event_timer_scheduler_t;

/**
 * Timer Schedulerを初期化し、slot storageを空にする。
 *
 * schedulerとstorageは再初期化または利用終了まで有効に保つ。
 *
 * @param scheduler 初期化するTimer Scheduler。
 * @param storage Timerを保持する呼出側所有slot配列。
 * @param capacity storageのslot数。
 * @return UT_EVENT_OKまたはUT_EVENT_INVALID_ARGUMENT。
 */
ut_event_result_t ut_event_timer_scheduler_init(ut_event_timer_scheduler_t *scheduler, ut_event_timer_slot_t *storage, size_t capacity);

/**
 * 未使用のTimer IDでTimerを開始する。
 *
 * event記述子はslotへ値copyするがpayload本体はcopyしない。periodが0ならone-shot、
 * 0より大きければperiodic Timerになる。deadlineはnow + delayで計算する。
 *
 * @param scheduler 初期化済みTimer Scheduler。
 * @param timer_id applicationが定義するTimer ID。
 * @param event deadline到達時にQueueへ発行するEvent。
 * @param now 現在時刻を表す単調増加tick。
 * @param delay 最初の発火までのtick数。0なら次回processで即時発火する。
 * @param period 周期tick。one-shotなら0。
 * @return UT_EVENT_OK、UT_EVENT_ALREADY_EXISTS、UT_EVENT_FULL、
 * UT_EVENT_BUSY、UT_EVENT_INVALID_ARGUMENT。
 */
ut_event_result_t ut_event_timer_start(ut_event_timer_scheduler_t *scheduler, ut_event_timer_id_t timer_id, const ut_event_t *event, uint64_t now, uint64_t delay, uint64_t period);

/**
 * activeなTimerのEventと時刻設定を置き換える。
 *
 * @param scheduler 初期化済みTimer Scheduler。
 * @param timer_id 再設定するTimer ID。
 * @param event deadline到達時にQueueへ発行するEvent。
 * @param now 現在時刻を表す単調増加tick。
 * @param delay 最初の発火までのtick数。
 * @param period 周期tick。one-shotなら0。
 * @return UT_EVENT_OK、UT_EVENT_NOT_FOUND、UT_EVENT_BUSY、
 * UT_EVENT_INVALID_ARGUMENT。
 */
ut_event_result_t ut_event_timer_restart(ut_event_timer_scheduler_t *scheduler, ut_event_timer_id_t timer_id, const ut_event_t *event, uint64_t now, uint64_t delay, uint64_t period);

/**
 * activeなTimerを取消し、slotを解放する。
 *
 * @param scheduler 初期化済みTimer Scheduler。
 * @param timer_id 取消すTimer ID。
 * @return UT_EVENT_OK、UT_EVENT_NOT_FOUND、UT_EVENT_BUSY、
 * UT_EVENT_INVALID_ARGUMENT。
 */
ut_event_result_t ut_event_timer_cancel(ut_event_timer_scheduler_t *scheduler, ut_event_timer_id_t timer_id);

/**
 * deadlineへ到達したTimer EventをQueueへ発行する。
 *
 * slot index順に処理し、1回の呼出しで各periodic Timerから最大1 Eventを発行する。
 * 遅延したperiodic Timerは過去回数分をburst発行せず、次の未来deadlineまで進める。
 * Queue満杯時はUT_EVENT_FULLを返し、未発行Timerをactiveなまま残す。
 *
 * @param scheduler 初期化済みTimer Scheduler。
 * @param now 現在時刻を表す単調増加tick。
 * @param queue Timer Eventの発行先Queue。
 * @param out_emitted_count 発行件数の任意の格納先。不要ならNULL。
 * @return UT_EVENT_OK、UT_EVENT_FULL、UT_EVENT_BUSY、
 * UT_EVENT_INVALID_ARGUMENT。
 */
ut_event_result_t ut_event_timer_process(ut_event_timer_scheduler_t *scheduler, uint64_t now, ut_event_queue_t *queue, size_t *out_emitted_count);

/**
 * activeなTimer件数を返す。
 *
 * @param scheduler Timer Scheduler。
 * @return active件数。無効なcontextでは0。
 */
size_t ut_event_timer_count(const ut_event_timer_scheduler_t *scheduler);

/**
 * 最も早いdeadlineを返す。
 *
 * @param scheduler 初期化済みTimer Scheduler。
 * @param out_deadline 最短deadlineの格納先。
 * @return UT_EVENT_OK、UT_EVENT_NOT_FOUND、UT_EVENT_INVALID_ARGUMENT。
 */
ut_event_result_t ut_event_timer_next_deadline(const ut_event_timer_scheduler_t *scheduler, uint64_t *out_deadline);

#ifdef __cplusplus
}
#endif

#endif
