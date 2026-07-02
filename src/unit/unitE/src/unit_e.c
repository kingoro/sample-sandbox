#include "unit_e.h"

#include <stdio.h>
#include <unistd.h>

/**
 * @file unit_e.c
 * @brief Unit Eのサンプル実装。
 *
 * この実装は、実際のセンサやモーターを操作する代わりに、呼び出された関数名と引数を
 * `printf` で出力します。`domain` から `unit` への呼び出しを追うための仮実装です。
 */

/**
 * @brief Unit Eの現在状態。
 */
static unit_e_status_t g_unit_e_status = UNIT_E_STATUS_NOT_INITIALIZED;

/**
 * @brief Unit Eの処理時間を模擬する。
 *
 * 実機操作には時間がかかるため、このサンプルでは各操作で1秒待ちます。
 */
static void sleep_unit_e_operation(void)
{
    (void)sleep(1U);
}

/**
 * @copydoc initialize_unit_e
 */
bool initialize_unit_e(void)
{
    sleep_unit_e_operation();
    printf("[unitE] initialize_unit_e()\n");
    g_unit_e_status = UNIT_E_STATUS_READY;
    return true;
}

/**
 * @copydoc terminate_unit_e
 */
bool terminate_unit_e(void)
{
    sleep_unit_e_operation();
    printf("[unitE] terminate_unit_e()\n");
    g_unit_e_status = UNIT_E_STATUS_NOT_INITIALIZED;
    return true;
}

/**
 * @copydoc get_unit_e_status
 */
bool get_unit_e_status(unit_e_status_t *status)
{
    sleep_unit_e_operation();

    if (status == NULL) {
        printf("[unitE] get_unit_e_status(status=NULL) -> false\n");
        return false;
    }

    *status = g_unit_e_status;
    printf("[unitE] get_unit_e_status(status=%s) -> true\n",
           convert_unit_e_status_to_string(g_unit_e_status));
    return true;
}

/**
 * @copydoc convert_unit_e_status_to_string
 */
const char *convert_unit_e_status_to_string(unit_e_status_t status)
{
    switch (status) {
    case UNIT_E_STATUS_NOT_INITIALIZED:
        return "NOT_INITIALIZED";
    case UNIT_E_STATUS_READY:
        return "READY";
    case UNIT_E_STATUS_OPEN:
        return "OPEN";
    case UNIT_E_STATUS_MOVING:
        return "MOVING";
    case UNIT_E_STATUS_CLOSED:
        return "CLOSED";
    default:
        return "UNKNOWN";
    }
}

/**
 * @copydoc move_unit_e
 */
bool move_unit_e(int position)
{
    sleep_unit_e_operation();

    if (position < 0) {
        printf("[unitE] move_unit_e(position=%d) -> false\n", position);
        return false;
    }

    printf("[unitE] move_unit_e(position=%d)\n", position);
    g_unit_e_status = UNIT_E_STATUS_MOVING;
    return true;
}

/**
 * @copydoc open_unit_e
 */
bool open_unit_e(void)
{
    sleep_unit_e_operation();
    printf("[unitE] open_unit_e()\n");
    g_unit_e_status = UNIT_E_STATUS_OPEN;
    return true;
}

/**
 * @copydoc close_unit_e
 */
bool close_unit_e(void)
{
    sleep_unit_e_operation();
    printf("[unitE] close_unit_e()\n");
    g_unit_e_status = UNIT_E_STATUS_CLOSED;
    return true;
}
