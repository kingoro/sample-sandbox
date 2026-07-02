#include "unit_a.h"

/**
 * @file test_unit_a.c
 * @brief Unit Aを単体で呼び出す確認用プログラム。
 */

/**
 * @brief Unit Aの基本操作を順番に呼び出す。
 *
 * @return 正常終了時は0。
 */
int main(void)
{
    unit_a_status_t status;

    (void)initialize_unit_a();
    (void)get_unit_a_status(&status);
    (void)open_unit_a();
    (void)move_unit_a(100);
    (void)close_unit_a();
    (void)get_unit_a_status(&status);
    (void)terminate_unit_a();
    return 0;
}
