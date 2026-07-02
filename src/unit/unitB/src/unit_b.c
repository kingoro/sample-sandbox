#include "unit_b.h"

#include <stdio.h>
#include <unistd.h>

/**
 * @file unit_b.c
 * @brief Unit Bのサンプル実装。
 *
 * この実装は、実際のセンサやモーターを操作する代わりに、呼び出された関数名と引数を
 * `printf` で出力します。`domain` から `unit` への呼び出しを追うための仮実装です。
 */

/**
 * @brief Unit Bの現在状態。
 */
static unit_b_status_t g_unit_b_status = UNIT_B_STATUS_NOT_INITIALIZED;

/**
 * @brief Unit Bの処理時間を模擬する。
 *
 * 実機操作には時間がかかるため、このサンプルでは各操作で1秒待ちます。
 */
static void sleep_unit_b_operation(void)
{
    (void)sleep(1U);
}

/**
 * @brief Unit Bを初期化する。
 *
 * @return 常に `true`。
 */
bool initialize_unit_b(void)
{
    sleep_unit_b_operation();
    printf("[unitB] initialize_unit_b()\n");
    g_unit_b_status = UNIT_B_STATUS_READY;
    return true;
}

/**
 * @brief Unit Bを終了する。
 *
 * @return 常に `true`。
 */
bool terminate_unit_b(void)
{
    sleep_unit_b_operation();
    printf("[unitB] terminate_unit_b()\n");
    g_unit_b_status = UNIT_B_STATUS_NOT_INITIALIZED;
    return true;
}

/**
 * @brief Unit Bの現在状態を取得する。
 *
 * @param status 現在状態を書き込む出力先。
 * @return 状態を取得できた場合は `true`、出力先が `NULL` の場合は `false`。
 */
bool get_unit_b_status(unit_b_status_t *status)
{
    sleep_unit_b_operation();

    if (status == NULL) {
        printf("[unitB] get_unit_b_status(status=NULL) -> false\n");
        return false;
    }

    *status = g_unit_b_status;
    printf("[unitB] get_unit_b_status(status=%s) -> true\n",
           convert_unit_b_status_to_string(g_unit_b_status));
    return true;
}

/**
 * @brief Unit Bの状態値を表示用文字列へ変換する。
 *
 * @param status 文字列へ変換する状態値。
 * @return 状態を表す静的文字列。
 */
const char *convert_unit_b_status_to_string(unit_b_status_t status)
{
    switch (status) {
    case UNIT_B_STATUS_NOT_INITIALIZED:
        return "NOT_INITIALIZED";
    case UNIT_B_STATUS_READY:
        return "READY";
    case UNIT_B_STATUS_OPEN:
        return "OPEN";
    case UNIT_B_STATUS_MOVING:
        return "MOVING";
    case UNIT_B_STATUS_CLOSED:
        return "CLOSED";
    default:
        return "UNKNOWN";
    }
}

/**
 * @brief Unit Bを指定位置へ移動する。
 *
 * @param position 移動先を表すサンプル値。
 * @return 常に `true`。
 */
bool move_unit_b(int position)
{
    sleep_unit_b_operation();

    if (position < 0) {
        printf("[unitB] move_unit_b(position=%d) -> false\n", position);
        return false;
    }

    printf("[unitB] move_unit_b(position=%d)\n", position);
    g_unit_b_status = UNIT_B_STATUS_MOVING;
    return true;
}

/**
 * @brief Unit Bを開く。
 *
 * @return 常に `true`。
 */
bool open_unit_b(void)
{
    sleep_unit_b_operation();
    printf("[unitB] open_unit_b()\n");
    g_unit_b_status = UNIT_B_STATUS_OPEN;
    return true;
}

/**
 * @brief Unit Bを閉じる。
 *
 * @return 常に `true`。
 */
bool close_unit_b(void)
{
    sleep_unit_b_operation();
    printf("[unitB] close_unit_b()\n");
    g_unit_b_status = UNIT_B_STATUS_CLOSED;
    return true;
}
