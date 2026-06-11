/**
 * @file utility_byte_result.h
 * @brief Byte Utilityのresult code。
 */
#ifndef UTILITY_BYTE_RESULT_H
#define UTILITY_BYTE_RESULT_H

#include <stdint.h>

/** Byte Utilityのresult code型。 */
typedef uint32_t ut_byte_result_t;

/** Byte Utilityのresult code。 */
enum {
    /** 操作が成功した。 */
    UT_BYTE_OK = 0u,
    /** NULL pointerまたは不正なcontextが渡された。 */
    UT_BYTE_INVALID_ARGUMENT = 1u,
    /** bufferの残り容量が不足している。 */
    UT_BYTE_OUT_OF_BOUNDS = 2u
};

#endif
