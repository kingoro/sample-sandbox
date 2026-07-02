#include "domain_step.h"

#include "unit_a.h"
#include "unit_b.h"
#include "unit_c.h"
#include "unit_d.h"
#include "unit_e.h"

#include <stdio.h>

/**
 * @file domain_step.c
 * @brief StepをUnit操作Actionへ変換して実行する。
 *
 * RunnerはUnitを知りません。
 * Unitを知っているのは、このStep実装だけです。
 *
 * このファイルを読むときのポイント:
 *
 * - `enqueue_domain_step_action()` は、StepをAction Queueへ積むための関数です。
 * - `execute_domain_step()` は、RunnerからActionとして呼ばれる関数です。
 * - `execute_domain_step_unit_a()` などは、Stepの内容をUnit関数へ変換する関数です。
 *
 * つまり、このファイルは「抽象的なStep」と「具体的なUnit API」の変換場所です。
 */

/**
 * @brief Actionとして実行されるStep共通関数。
 *
 * Runnerはこの関数を関数ポインタ経由で呼びます。
 * `context` には `domain_step_t` のポインタが入っています。
 */
static bool execute_domain_step(void *context);

/**
 * @brief Unit A向けStepをUnit A API呼び出しへ変換する。
 */
static bool execute_domain_step_unit_a(const domain_step_t *step);

/**
 * @brief Unit B向けStepをUnit B API呼び出しへ変換する。
 */
static bool execute_domain_step_unit_b(const domain_step_t *step);

/**
 * @brief Unit C向けStepをUnit C API呼び出しへ変換する。
 */
static bool execute_domain_step_unit_c(const domain_step_t *step);

/**
 * @brief Unit D向けStepをUnit D API呼び出しへ変換する。
 */
static bool execute_domain_step_unit_d(const domain_step_t *step);

/**
 * @brief Unit E向けStepをUnit E API呼び出しへ変換する。
 */
static bool execute_domain_step_unit_e(const domain_step_t *step);

/**
 * @copydoc enqueue_domain_step_action
 *
 * ここではUnit関数をまだ呼びません。
 * Stepを「あとでRunnerが実行できるAction」に包み直して、Queueへ積むだけです。
 */
bool enqueue_domain_step_action(
    domain_action_queue_t *queue,
    const domain_step_t *step,
    const char *scenario_name,
    const char *sequence_name,
    size_t step_index,
    size_t total_step_count)
{
    domain_action_t action;

    if ((queue == NULL) || (step == NULL)) {
        return false;
    }

    /*
     * Actionの中には、実行関数と実行時に使うcontextを入れます。
     * 今回のcontextはStepそのものです。
     * Runnerはcontextの中身を知りません。
     */
    action.name = step->name;
    action.scenario_name = scenario_name;
    action.sequence_name = sequence_name;
    action.step_name = step->name;
    action.step_index = step_index;
    action.total_step_count = total_step_count;
    action.execute = execute_domain_step;
    action.context = (void *)step;

    return push_domain_action_queue(queue, &action);
}

static bool execute_domain_step(void *context)
{
    const domain_step_t *step = (const domain_step_t *)context;

    if (step == NULL) {
        return false;
    }

    printf("[domain] execute_domain_step(name=%s, unit=%d, operation=%d, value=%d)\n",
           (step->name != NULL) ? step->name : "UNKNOWN",
           (int)step->unit,
           (int)step->operation,
           step->value);

    /*
     * ここで初めて「どのUnitへ渡すStepか」を見ます。
     * RunnerではなくStep層でUnit振り分けをすることで、
     * Runnerを汎用的な実行器として保てます。
     */
    switch (step->unit) {
    case DOMAIN_STEP_UNIT_A:
        return execute_domain_step_unit_a(step);
    case DOMAIN_STEP_UNIT_B:
        return execute_domain_step_unit_b(step);
    case DOMAIN_STEP_UNIT_C:
        return execute_domain_step_unit_c(step);
    case DOMAIN_STEP_UNIT_D:
        return execute_domain_step_unit_d(step);
    case DOMAIN_STEP_UNIT_E:
        return execute_domain_step_unit_e(step);
    default:
        return false;
    }
}

static bool execute_domain_step_unit_a(const domain_step_t *step)
{
    unit_a_status_t status;

    /*
     * operationをUnit Aの公開APIへ対応付けます。
     * たとえばOPENならopen_unit_a()、MOVEならmove_unit_a(value)です。
     */
    switch (step->operation) {
    case DOMAIN_STEP_OPERATION_INITIALIZE:
        return initialize_unit_a();
    case DOMAIN_STEP_OPERATION_TERMINATE:
        return terminate_unit_a();
    case DOMAIN_STEP_OPERATION_GET_STATUS:
        return get_unit_a_status(&status);
    case DOMAIN_STEP_OPERATION_MOVE:
        return move_unit_a(step->value);
    case DOMAIN_STEP_OPERATION_OPEN:
        return open_unit_a();
    case DOMAIN_STEP_OPERATION_CLOSE:
        return close_unit_a();
    default:
        return false;
    }
}

static bool execute_domain_step_unit_b(const domain_step_t *step)
{
    unit_b_status_t status;

    /*
     * Unit BもUnit Aと同じ形です。
     * 実プロジェクトでは、この中でUnit B固有のAPIへ接続します。
     */
    switch (step->operation) {
    case DOMAIN_STEP_OPERATION_INITIALIZE:
        return initialize_unit_b();
    case DOMAIN_STEP_OPERATION_TERMINATE:
        return terminate_unit_b();
    case DOMAIN_STEP_OPERATION_GET_STATUS:
        return get_unit_b_status(&status);
    case DOMAIN_STEP_OPERATION_MOVE:
        return move_unit_b(step->value);
    case DOMAIN_STEP_OPERATION_OPEN:
        return open_unit_b();
    case DOMAIN_STEP_OPERATION_CLOSE:
        return close_unit_b();
    default:
        return false;
    }
}

static bool execute_domain_step_unit_c(const domain_step_t *step)
{
    unit_c_status_t status;

    /*
     * サンプルでは全Unitを同じ操作セットにしています。
     * Unitごとに使える操作が違う場合は、ここで未対応操作をfalseにします。
     */
    switch (step->operation) {
    case DOMAIN_STEP_OPERATION_INITIALIZE:
        return initialize_unit_c();
    case DOMAIN_STEP_OPERATION_TERMINATE:
        return terminate_unit_c();
    case DOMAIN_STEP_OPERATION_GET_STATUS:
        return get_unit_c_status(&status);
    case DOMAIN_STEP_OPERATION_MOVE:
        return move_unit_c(step->value);
    case DOMAIN_STEP_OPERATION_OPEN:
        return open_unit_c();
    case DOMAIN_STEP_OPERATION_CLOSE:
        return close_unit_c();
    default:
        return false;
    }
}

static bool execute_domain_step_unit_d(const domain_step_t *step)
{
    unit_d_status_t status;

    /*
     * Stepから見ると「Unit Dへ何かを依頼する」だけです。
     * 具体的なUnit Dの実装詳細はunit層の責務です。
     */
    switch (step->operation) {
    case DOMAIN_STEP_OPERATION_INITIALIZE:
        return initialize_unit_d();
    case DOMAIN_STEP_OPERATION_TERMINATE:
        return terminate_unit_d();
    case DOMAIN_STEP_OPERATION_GET_STATUS:
        return get_unit_d_status(&status);
    case DOMAIN_STEP_OPERATION_MOVE:
        return move_unit_d(step->value);
    case DOMAIN_STEP_OPERATION_OPEN:
        return open_unit_d();
    case DOMAIN_STEP_OPERATION_CLOSE:
        return close_unit_d();
    default:
        return false;
    }
}

static bool execute_domain_step_unit_e(const domain_step_t *step)
{
    unit_e_status_t status;

    /*
     * Unit E用の変換です。
     * Unitが増える場合は、enum追加とこのような変換関数追加が必要になります。
     */
    switch (step->operation) {
    case DOMAIN_STEP_OPERATION_INITIALIZE:
        return initialize_unit_e();
    case DOMAIN_STEP_OPERATION_TERMINATE:
        return terminate_unit_e();
    case DOMAIN_STEP_OPERATION_GET_STATUS:
        return get_unit_e_status(&status);
    case DOMAIN_STEP_OPERATION_MOVE:
        return move_unit_e(step->value);
    case DOMAIN_STEP_OPERATION_OPEN:
        return open_unit_e();
    case DOMAIN_STEP_OPERATION_CLOSE:
        return close_unit_e();
    default:
        return false;
    }
}
