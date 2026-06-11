/**
 * @file utility_event_timer.c
 * @brief 外部clock駆動Timer Schedulerの実装。
 */
#include "utility_event_timer.h"

#include <string.h>

/**
 * Timer Schedulerの内部不変条件を検査する。
 *
 * @param scheduler 検査するTimer Scheduler。
 * @return 利用可能なら真、それ以外は偽。
 */
static int scheduler_is_valid(
    const ut_event_timer_scheduler_t *scheduler)
{
    return (scheduler != NULL) &&
        (scheduler->slots != NULL) &&
        (scheduler->capacity > 0u) &&
        (scheduler->count <= scheduler->capacity);
}

/**
 * Timer IDに一致するactive slotを探索する。
 *
 * @param scheduler 初期化済みTimer Scheduler。
 * @param timer_id 探索するTimer ID。
 * @return 一致したslot。見つからない場合NULL。
 */
static ut_event_timer_slot_t *find_slot(
    ut_event_timer_scheduler_t *scheduler,
    ut_event_timer_id_t timer_id)
{
    size_t index;

    for (index = 0u; index < scheduler->capacity; index++) {
        if ((scheduler->slots[index].active != 0u) &&
            (scheduler->slots[index].id == timer_id)) {
            return &scheduler->slots[index];
        }
    }
    return NULL;
}

/**
 * 最初の未使用slotを探索する。
 *
 * @param scheduler 初期化済みTimer Scheduler。
 * @return 未使用slot。見つからない場合NULL。
 */
static ut_event_timer_slot_t *find_free_slot(
    ut_event_timer_scheduler_t *scheduler)
{
    size_t index;

    for (index = 0u; index < scheduler->capacity; index++) {
        if (scheduler->slots[index].active == 0u) {
            return &scheduler->slots[index];
        }
    }
    return NULL;
}

/**
 * now + delayをoverflow検査付きで計算する。
 *
 * @param now 現在tick。
 * @param delay 加算するtick数。
 * @param out_deadline 計算結果の格納先。
 * @return 計算可能なら真、それ以外は偽。
 */
static int calculate_deadline(
    uint64_t now,
    uint64_t delay,
    uint64_t *out_deadline)
{
    if ((out_deadline == NULL) || (delay > (UINT64_MAX - now))) {
        return 0;
    }
    *out_deadline = now + delay;
    return 1;
}

/**
 * Timer slotへEventと時刻設定を格納する。
 *
 * @param slot 設定するslot。
 * @param timer_id Timer ID。
 * @param event 発行するEvent。
 * @param deadline 最初のdeadline。
 * @param period 周期tick。
 */
static void configure_slot(
    ut_event_timer_slot_t *slot,
    ut_event_timer_id_t timer_id,
    const ut_event_t *event,
    uint64_t deadline,
    uint64_t period)
{
    slot->id = timer_id;
    slot->event = *event;
    slot->deadline = deadline;
    slot->period = period;
    slot->active = 1u;
}

/**
 * periodic Timerのdeadlineをnowより後へ進める。
 *
 * uint64_t範囲内に次回deadlineを表現できない場合はTimerを停止する。
 *
 * @param slot 発火済みperiodic Timer slot。
 * @param now 現在tick。
 * @return Timerを継続できる場合真、停止した場合偽。
 */
static int advance_periodic_deadline(
    ut_event_timer_slot_t *slot,
    uint64_t now)
{
    const uint64_t elapsed = now - slot->deadline;
    const uint64_t elapsed_periods = elapsed / slot->period;
    uint64_t steps;

    if (elapsed_periods == UINT64_MAX) {
        return 0;
    }
    steps = elapsed_periods + 1u;
    if (steps > ((UINT64_MAX - slot->deadline) / slot->period)) {
        return 0;
    }
    slot->deadline += steps * slot->period;
    return 1;
}

ut_event_result_t ut_event_timer_scheduler_init(
    ut_event_timer_scheduler_t *scheduler,
    ut_event_timer_slot_t *storage,
    size_t capacity)
{
    if ((scheduler == NULL) || (storage == NULL) || (capacity == 0u) ||
        (capacity > (SIZE_MAX / sizeof(storage[0])))) {
        return UT_EVENT_INVALID_ARGUMENT;
    }

    (void)memset(storage, 0, capacity * sizeof(storage[0]));
    scheduler->slots = storage;
    scheduler->capacity = capacity;
    scheduler->count = 0u;
    scheduler->processing = 0u;
    return UT_EVENT_OK;
}

ut_event_result_t ut_event_timer_start(
    ut_event_timer_scheduler_t *scheduler,
    ut_event_timer_id_t timer_id,
    const ut_event_t *event,
    uint64_t now,
    uint64_t delay,
    uint64_t period)
{
    ut_event_timer_slot_t *slot;
    uint64_t deadline;

    if (!scheduler_is_valid(scheduler) || (event == NULL) ||
        !calculate_deadline(now, delay, &deadline)) {
        return UT_EVENT_INVALID_ARGUMENT;
    }
    if (scheduler->processing != 0u) {
        return UT_EVENT_BUSY;
    }
    if (find_slot(scheduler, timer_id) != NULL) {
        return UT_EVENT_ALREADY_EXISTS;
    }

    slot = find_free_slot(scheduler);
    if (slot == NULL) {
        return UT_EVENT_FULL;
    }
    configure_slot(slot, timer_id, event, deadline, period);
    scheduler->count++;
    return UT_EVENT_OK;
}

ut_event_result_t ut_event_timer_restart(
    ut_event_timer_scheduler_t *scheduler,
    ut_event_timer_id_t timer_id,
    const ut_event_t *event,
    uint64_t now,
    uint64_t delay,
    uint64_t period)
{
    ut_event_timer_slot_t *slot;
    uint64_t deadline;

    if (!scheduler_is_valid(scheduler) || (event == NULL) ||
        !calculate_deadline(now, delay, &deadline)) {
        return UT_EVENT_INVALID_ARGUMENT;
    }
    if (scheduler->processing != 0u) {
        return UT_EVENT_BUSY;
    }

    slot = find_slot(scheduler, timer_id);
    if (slot == NULL) {
        return UT_EVENT_NOT_FOUND;
    }
    configure_slot(slot, timer_id, event, deadline, period);
    return UT_EVENT_OK;
}

ut_event_result_t ut_event_timer_cancel(
    ut_event_timer_scheduler_t *scheduler,
    ut_event_timer_id_t timer_id)
{
    ut_event_timer_slot_t *slot;

    if (!scheduler_is_valid(scheduler)) {
        return UT_EVENT_INVALID_ARGUMENT;
    }
    if (scheduler->processing != 0u) {
        return UT_EVENT_BUSY;
    }

    slot = find_slot(scheduler, timer_id);
    if (slot == NULL) {
        return UT_EVENT_NOT_FOUND;
    }
    (void)memset(slot, 0, sizeof(*slot));
    scheduler->count--;
    return UT_EVENT_OK;
}

ut_event_result_t ut_event_timer_process(
    ut_event_timer_scheduler_t *scheduler,
    uint64_t now,
    ut_event_queue_t *queue,
    size_t *out_emitted_count)
{
    size_t index;
    size_t emitted_count = 0u;
    ut_event_result_t result = UT_EVENT_OK;

    if (!scheduler_is_valid(scheduler) || (queue == NULL)) {
        return UT_EVENT_INVALID_ARGUMENT;
    }
    if (scheduler->processing != 0u) {
        return UT_EVENT_BUSY;
    }

    scheduler->processing = 1u;
    for (index = 0u; index < scheduler->capacity; index++) {
        ut_event_timer_slot_t *slot = &scheduler->slots[index];

        if ((slot->active == 0u) || (now < slot->deadline)) {
            continue;
        }

        result = ut_event_queue_push(queue, &slot->event);
        if (result != UT_EVENT_OK) {
            break;
        }
        emitted_count++;

        if (slot->period == 0u) {
            (void)memset(slot, 0, sizeof(*slot));
            scheduler->count--;
        } else if (!advance_periodic_deadline(slot, now)) {
            (void)memset(slot, 0, sizeof(*slot));
            scheduler->count--;
        }
    }
    scheduler->processing = 0u;

    if (out_emitted_count != NULL) {
        *out_emitted_count = emitted_count;
    }
    return result;
}

size_t ut_event_timer_count(
    const ut_event_timer_scheduler_t *scheduler)
{
    return scheduler_is_valid(scheduler) ? scheduler->count : 0u;
}

ut_event_result_t ut_event_timer_next_deadline(
    const ut_event_timer_scheduler_t *scheduler,
    uint64_t *out_deadline)
{
    size_t index;
    uint64_t deadline = UINT64_MAX;
    int found = 0;

    if (!scheduler_is_valid(scheduler) || (out_deadline == NULL)) {
        return UT_EVENT_INVALID_ARGUMENT;
    }

    for (index = 0u; index < scheduler->capacity; index++) {
        const ut_event_timer_slot_t *slot = &scheduler->slots[index];

        if ((slot->active != 0u) && (!found || (slot->deadline < deadline))) {
            deadline = slot->deadline;
            found = 1;
        }
    }
    if (!found) {
        return UT_EVENT_NOT_FOUND;
    }
    *out_deadline = deadline;
    return UT_EVENT_OK;
}
