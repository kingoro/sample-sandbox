/**
 * @file utility_event_types.h
 * @brief Event記述子と共通Event IDを定義する。
 */
#ifndef UTILITY_EVENT_TYPES_H
#define UTILITY_EVENT_TYPES_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * すべてのEvent IDに一致する購読用ID。
 * application固有のEvent IDとしては使用しない。
 */
#define UT_EVENT_ID_ANY UINT32_MAX

/**
 * Queueへ値として格納されるイベント記述子。
 *
 * payloadは参照先を所有しない。payloadを使用する場合、イベントの処理完了まで
 * 参照先を有効に保つ責務は呼出側にある。
 */
typedef struct ut_event {
    /** Eventの種類。ID体系とpayload型の対応はapplication側で定義する。 */
    uint32_t id;
    /** Eventを発行したmoduleの識別子。不要な場合は0を使用できる。 */
    uint32_t source;
    /** Queueが所有しないpayloadへの参照。payloadがなければNULL。 */
    const void *payload;
    /** payloadが指す領域のbyte数。payloadがNULLの場合は0とする。 */
    size_t payload_size;
} ut_event_t;

#ifdef __cplusplus
}
#endif

#endif
