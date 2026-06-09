/**
 * @file memory_buffer_event_adapter.h
 * @brief Memory BufferをEvent Buffer Pool抽象へ接続するadapter API。
 */
#ifndef MEMORY_BUFFER_EVENT_ADAPTER_H
#define MEMORY_BUFFER_EVENT_ADAPTER_H

#include "memory_buffer.h"
#include "utility_event_buffer.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Memory Buffer contextをEvent Buffer Pool操作tableとして公開する。
 *
 * 返却tableはmb_context_tへの非所有pointerを保持する。contextとarenaは、返却tableを
 * 使用する全Envelopeがreleaseされるまで有効に保つ。同じcontextへの並行accessは
 * 呼出側が直列化する。
 *
 * @param context 初期化済みMemory Buffer context。
 * @return Event Buffer Pool操作table。contextがNULLでもtableは返るが操作は失敗する。
 */
ut_event_buffer_pool_t mb_event_buffer_pool(mb_context_t *context);

#ifdef __cplusplus
}
#endif

#endif
