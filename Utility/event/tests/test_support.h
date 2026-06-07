/**
 * @file test_support.h
 * @brief Event Utility単体テスト共通の検証macro。
 */
#ifndef UTILITY_EVENT_TEST_SUPPORT_H
#define UTILITY_EVENT_TEST_SUPPORT_H

#include <stdio.h>

/**
 * 条件が偽の場合に失敗場所を表示して現在のtest関数を失敗させる。
 *
 * @param condition 検証する真偽式。
 */
#define CHECK(condition)                                                      \
    do {                                                                      \
        if (!(condition)) {                                                   \
            (void)fprintf(                                                    \
                stderr, "%s:%d: CHECK failed: %s\n",                         \
                __FILE__, __LINE__, #condition);                              \
            return 1;                                                         \
        }                                                                     \
    } while (0)

#endif
