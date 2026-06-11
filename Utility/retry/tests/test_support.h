/**
 * @file test_support.h
 * @brief Retry Utility test用検証macro。
 */
#ifndef UTILITY_RETRY_TEST_SUPPORT_H
#define UTILITY_RETRY_TEST_SUPPORT_H
#include <stdio.h>
/** @brief 条件を検証する。 @param condition 真偽式。 */
#define CHECK(condition) do { if (!(condition)) { (void)fprintf(stderr, \
    "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #condition); return 1; } } while (0)
#endif
