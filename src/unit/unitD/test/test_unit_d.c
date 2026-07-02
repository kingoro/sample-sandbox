#include "unit_d.h"

/**
 * @file test_unit_d.c
 * @brief Unit Dを単体で呼び出す確認用プログラム。
 */

/**
 * @brief Unit Dの基本操作を順番に呼び出す。
 *
 * @return 正常終了時は0。
 */
int main(void)
{
    unit_d_status_t status;

    (void)initialize_unit_d();
    (void)get_unit_d_status(&status);
    (void)open_unit_d();
    (void)move_unit_d(400);
    (void)close_unit_d();
    (void)get_unit_d_status(&status);
    (void)terminate_unit_d();
    return 0;
}
