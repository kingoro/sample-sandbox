#ifndef UTILITY_EVENT_TEST_SUPPORT_H
#define UTILITY_EVENT_TEST_SUPPORT_H

#include <stdio.h>

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
