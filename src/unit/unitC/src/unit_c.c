#include "unit_c.h"

#include <stdio.h>
#include <unistd.h>

/**
 * @file unit_c.c
 * @brief Unit Cのサンプル実装。
 *
 * この実装は、実際のセンサやモーターを操作する代わりに、呼び出された関数名と引数を
 * `printf` で出力します。`domain` から `unit` への呼び出しを追うための仮実装です。
 */

/**
 * @brief Unit Cの現在状態。
 */
static unit_c_status_t g_unit_c_status = UNIT_C_STATUS_NOT_INITIALIZED;

/**
 * @brief Unit Cの処理時間を模擬する。
 *
 * 実機操作には時間がかかるため、このサンプルでは各操作で1秒待ちます。
 */
static void sleep_unit_c_operation(void)
{
    (void)sleep(1U);
}

/**
 * @copydoc initialize_unit_c
 */
bool initialize_unit_c(void)
{
    sleep_unit_c_operation();
    printf("[unitC] initialize_unit_c()\n");
    g_unit_c_status = UNIT_C_STATUS_READY;
    return true;
}

/**
 * @copydoc terminate_unit_c
 */
bool terminate_unit_c(void)
{
    sleep_unit_c_operation();
    printf("[unitC] terminate_unit_c()\n");
    g_unit_c_status = UNIT_C_STATUS_NOT_INITIALIZED;
    return true;
}

/**
 * @copydoc get_unit_c_status
 */
bool get_unit_c_status(unit_c_status_t *status)
{
    sleep_unit_c_operation();

    if (status == NULL) {
        printf("[unitC] get_unit_c_status(status=NULL) -> false\n");
        return false;
    }

    *status = g_unit_c_status;
    printf("[unitC] get_unit_c_status(status=%s) -> true\n",
           convert_unit_c_status_to_string(g_unit_c_status));
    return true;
}

/**
 * @copydoc convert_unit_c_status_to_string
 */
const char *convert_unit_c_status_to_string(unit_c_status_t status)
{
    switch (status) {
    case UNIT_C_STATUS_NOT_INITIALIZED:
        return "NOT_INITIALIZED";
    case UNIT_C_STATUS_READY:
        return "READY";
    case UNIT_C_STATUS_OPEN:
        return "OPEN";
    case UNIT_C_STATUS_MOVING:
        return "MOVING";
    case UNIT_C_STATUS_CLOSED:
        return "CLOSED";
    default:
        return "UNKNOWN";
    }
}

/**
 * @copydoc move_unit_c
 */
bool move_unit_c(int position)
{
    sleep_unit_c_operation();

    if (position < 0) {
        printf("[unitC] move_unit_c(position=%d) -> false\n", position);
        return false;
    }

    printf("[unitC] move_unit_c(position=%d)\n", position);
    g_unit_c_status = UNIT_C_STATUS_MOVING;
    return true;
}

/**
 * @copydoc open_unit_c
 */
bool open_unit_c(void)
{
    sleep_unit_c_operation();
    printf("[unitC] open_unit_c()\n");
    g_unit_c_status = UNIT_C_STATUS_OPEN;
    return true;
}

/**
 * @copydoc close_unit_c
 */
bool close_unit_c(void)
{
    sleep_unit_c_operation();
    printf("[unitC] close_unit_c()\n");
    g_unit_c_status = UNIT_C_STATUS_CLOSED;
    return true;
}
