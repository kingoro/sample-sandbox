#include "unit_a.h"

#include <stdio.h>
#include <unistd.h>

/**
 * @file unit_a.c
 * @brief Unit Aのサンプル実装。
 *
 * この実装は、実際のセンサやモーターを操作する代わりに、呼び出された関数名と引数を
 * `printf` で出力します。`domain` から `unit` への呼び出しを追うための仮実装です。
 */

/**
 * @brief Unit Aの現在状態。
 *
 * サンプルを単純にするため、Unit Aの状態はこのファイル内の静的変数で保持します。
 */
static unit_a_status_t g_unit_a_status = UNIT_A_STATUS_NOT_INITIALIZED;

/**
 * @brief Unit Aの処理時間を模擬する。
 *
 * 実機操作には時間がかかるため、このサンプルでは各操作で1秒待ちます。
 */
static void sleep_unit_a_operation(void)
{
    (void)sleep(1U);
}

/**
 * @brief Unit Aを初期化する。
 *
 * @return 常に `true`。
 */
bool initialize_unit_a(void)
{
    sleep_unit_a_operation();
    printf("[unitA] initialize_unit_a()\n");
    g_unit_a_status = UNIT_A_STATUS_READY;
    return true;
}

/**
 * @brief Unit Aを終了する。
 *
 * @return 常に `true`。
 */
bool terminate_unit_a(void)
{
    sleep_unit_a_operation();
    printf("[unitA] terminate_unit_a()\n");
    g_unit_a_status = UNIT_A_STATUS_NOT_INITIALIZED;
    return true;
}

/**
 * @brief Unit Aの現在状態を取得する。
 *
 * @param status 現在状態を書き込む出力先。
 * @return 状態を取得できた場合は `true`、出力先が `NULL` の場合は `false`。
 */
bool get_unit_a_status(unit_a_status_t *status)
{
    sleep_unit_a_operation();

    if (status == NULL) {
        printf("[unitA] get_unit_a_status(status=NULL) -> false\n");
        return false;
    }

    *status = g_unit_a_status;
    printf("[unitA] get_unit_a_status(status=%s) -> true\n",
           convert_unit_a_status_to_string(g_unit_a_status));
    return true;
}

/**
 * @brief Unit Aの状態値を表示用文字列へ変換する。
 *
 * @param status 文字列へ変換する状態値。
 * @return 状態を表す静的文字列。
 */
const char *convert_unit_a_status_to_string(unit_a_status_t status)
{
    switch (status) {
    case UNIT_A_STATUS_NOT_INITIALIZED:
        return "NOT_INITIALIZED";
    case UNIT_A_STATUS_READY:
        return "READY";
    case UNIT_A_STATUS_OPEN:
        return "OPEN";
    case UNIT_A_STATUS_MOVING:
        return "MOVING";
    case UNIT_A_STATUS_CLOSED:
        return "CLOSED";
    default:
        return "UNKNOWN";
    }
}

/**
 * @brief Unit Aを指定位置へ移動する。
 *
 * @param position 移動先を表すサンプル値。
 * @return 移動先が0以上の場合は `true`、負の値の場合は `false`。
 */
bool move_unit_a(int position)
{
    sleep_unit_a_operation();

    if (position < 0) {
        printf("[unitA] move_unit_a(position=%d) -> false\n", position);
        return false;
    }

    printf("[unitA] move_unit_a(position=%d)\n", position);
    g_unit_a_status = UNIT_A_STATUS_MOVING;
    return true;
}

/**
 * @brief Unit Aを開く。
 *
 * @return 常に `true`。
 */
bool open_unit_a(void)
{
    sleep_unit_a_operation();
    printf("[unitA] open_unit_a()\n");
    g_unit_a_status = UNIT_A_STATUS_OPEN;
    return true;
}

/**
 * @brief Unit Aを閉じる。
 *
 * @return 常に `true`。
 */
bool close_unit_a(void)
{
    sleep_unit_a_operation();
    printf("[unitA] close_unit_a()\n");
    g_unit_a_status = UNIT_A_STATUS_CLOSED;
    return true;
}
