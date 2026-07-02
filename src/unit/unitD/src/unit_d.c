#include "unit_d.h"

#include <stdio.h>
#include <unistd.h>

/**
 * @file unit_d.c
 * @brief Unit Dのサンプル実装。
 *
 * この実装は、実際のセンサやモーターを操作する代わりに、呼び出された関数名と引数を
 * `printf` で出力します。`domain` から `unit` への呼び出しを追うための仮実装です。
 */

/**
 * @brief Unit Dの現在状態。
 */
static unit_d_status_t g_unit_d_status = UNIT_D_STATUS_NOT_INITIALIZED;

/**
 * @brief Unit Dの処理時間を模擬する。
 *
 * 実機操作には時間がかかるため、このサンプルでは各操作で1秒待ちます。
 */
static void sleep_unit_d_operation(void)
{
    (void)sleep(1U);
}

/**
 * @copydoc initialize_unit_d
 */
bool initialize_unit_d(void)
{
    sleep_unit_d_operation();
    printf("[unitD] initialize_unit_d()\n");
    g_unit_d_status = UNIT_D_STATUS_READY;
    return true;
}

/**
 * @copydoc terminate_unit_d
 */
bool terminate_unit_d(void)
{
    sleep_unit_d_operation();
    printf("[unitD] terminate_unit_d()\n");
    g_unit_d_status = UNIT_D_STATUS_NOT_INITIALIZED;
    return true;
}

/**
 * @copydoc get_unit_d_status
 */
bool get_unit_d_status(unit_d_status_t *status)
{
    sleep_unit_d_operation();

    if (status == NULL) {
        printf("[unitD] get_unit_d_status(status=NULL) -> false\n");
        return false;
    }

    *status = g_unit_d_status;
    printf("[unitD] get_unit_d_status(status=%s) -> true\n",
           convert_unit_d_status_to_string(g_unit_d_status));
    return true;
}

/**
 * @copydoc convert_unit_d_status_to_string
 */
const char *convert_unit_d_status_to_string(unit_d_status_t status)
{
    switch (status) {
    case UNIT_D_STATUS_NOT_INITIALIZED:
        return "NOT_INITIALIZED";
    case UNIT_D_STATUS_READY:
        return "READY";
    case UNIT_D_STATUS_OPEN:
        return "OPEN";
    case UNIT_D_STATUS_MOVING:
        return "MOVING";
    case UNIT_D_STATUS_CLOSED:
        return "CLOSED";
    default:
        return "UNKNOWN";
    }
}

/**
 * @copydoc move_unit_d
 */
bool move_unit_d(int position)
{
    sleep_unit_d_operation();

    if (position < 0) {
        printf("[unitD] move_unit_d(position=%d) -> false\n", position);
        return false;
    }

    printf("[unitD] move_unit_d(position=%d)\n", position);
    g_unit_d_status = UNIT_D_STATUS_MOVING;
    return true;
}

/**
 * @copydoc open_unit_d
 */
bool open_unit_d(void)
{
    sleep_unit_d_operation();
    printf("[unitD] open_unit_d()\n");
    g_unit_d_status = UNIT_D_STATUS_OPEN;
    return true;
}

/**
 * @copydoc close_unit_d
 */
bool close_unit_d(void)
{
    sleep_unit_d_operation();
    printf("[unitD] close_unit_d()\n");
    g_unit_d_status = UNIT_D_STATUS_CLOSED;
    return true;
}
