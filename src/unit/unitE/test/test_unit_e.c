#include "unit_e.h"

/**
 * @file test_unit_e.c
 * @brief Unit Eを単体で呼び出す確認用プログラム。
 */

/**
 * @brief Unit Eの基本操作を順番に呼び出す。
 *
 * @return 正常終了時は0。
 */
int main(void)
{
    unit_e_status_t status;

    (void)initialize_unit_e();
    (void)get_unit_e_status(&status);
    (void)open_unit_e();
    (void)move_unit_e(500);
    (void)close_unit_e();
    (void)get_unit_e_status(&status);
    (void)terminate_unit_e();
    return 0;
}
