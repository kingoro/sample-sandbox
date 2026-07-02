#include "unit_b.h"

/**
 * @file test_unit_b.c
 * @brief Unit Bを単体で呼び出す確認用プログラム。
 */

/**
 * @brief Unit Bの基本操作を順番に呼び出す。
 *
 * @return 正常終了時は0。
 */
int main(void)
{
    unit_b_status_t status;

    (void)initialize_unit_b();
    (void)get_unit_b_status(&status);
    (void)open_unit_b();
    (void)move_unit_b(200);
    (void)close_unit_b();
    (void)get_unit_b_status(&status);
    (void)terminate_unit_b();
    return 0;
}
