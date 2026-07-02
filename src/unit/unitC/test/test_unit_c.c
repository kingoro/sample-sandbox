#include "unit_c.h"

/**
 * @file test_unit_c.c
 * @brief Unit Cを単体で呼び出す確認用プログラム。
 */

/**
 * @brief Unit Cの基本操作を順番に呼び出す。
 *
 * @return 正常終了時は0。
 */
int main(void)
{
    unit_c_status_t status;

    (void)initialize_unit_c();
    (void)get_unit_c_status(&status);
    (void)open_unit_c();
    (void)move_unit_c(300);
    (void)close_unit_c();
    (void)get_unit_c_status(&status);
    (void)terminate_unit_c();
    return 0;
}
