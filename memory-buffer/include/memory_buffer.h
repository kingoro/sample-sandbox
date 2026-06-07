#ifndef MEMORY_BUFFER_H
#define MEMORY_BUFFER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MB_CONTEXT_SIZE 4096u
#define MB_CONTEXT_ALIGNMENT 8u
#define MB_MAX_BUFFERS 64u
#define MB_ABI_VERSION_MAJOR 1u
#define MB_ABI_VERSION_MINOR 0u
#define MB_ABI_VERSION ((MB_ABI_VERSION_MAJOR << 16u) | MB_ABI_VERSION_MINOR)

/** 世代番号付きtoken。bit layoutはlibrary内部仕様とする。 */
typedef uint32_t mb_handle_t;

/** すべてのmemory-buffer操作が返す32bit状態code。 */
typedef uint32_t mb_result_t;

enum {
    MB_OK = 0,
    MB_INVALID_ARGUMENT = 1,
    MB_NOT_INITIALIZED = 2,
    MB_OUT_OF_MEMORY = 4,
    MB_INVALID_HANDLE = 5,
    MB_OUT_OF_BOUNDS = 6,
    MB_BUSY = 7
};

/** 静的確保できるopaqueな制御storage。 */
typedef union mb_context {
    uint64_t _alignment;
    uint8_t _storage[MB_CONTEXT_SIZE];
} mb_context_t;

/** 割り当て済みバッファの公開metadata。 */
typedef struct mb_buffer_info {
    size_t length;
    size_t capacity;
    uint8_t mapped;
    uint8_t reserved[7];
} mb_buffer_info_t;

#if defined(__cplusplus)
static_assert(sizeof(mb_context_t) == MB_CONTEXT_SIZE, "mb_context_t ABI mismatch");
static_assert(alignof(mb_context_t) >= MB_CONTEXT_ALIGNMENT, "mb_context_t alignment mismatch");
static_assert(sizeof(mb_result_t) == sizeof(uint32_t), "mb_result_t ABI mismatch");
static_assert(sizeof(mb_buffer_info_t) == (2u * sizeof(size_t)) + 8u, "mb_buffer_info_t size mismatch");
static_assert(offsetof(mb_buffer_info_t, length) == 0u, "mb_buffer_info_t.length offset mismatch");
static_assert(offsetof(mb_buffer_info_t, capacity) == sizeof(size_t), "mb_buffer_info_t.capacity offset mismatch");
static_assert(offsetof(mb_buffer_info_t, mapped) == 2u * sizeof(size_t), "mb_buffer_info_t.mapped offset mismatch");
#else
_Static_assert(sizeof(mb_context_t) == MB_CONTEXT_SIZE, "mb_context_t ABI mismatch");
_Static_assert(_Alignof(mb_context_t) >= MB_CONTEXT_ALIGNMENT, "mb_context_t alignment mismatch");
_Static_assert(sizeof(mb_result_t) == sizeof(uint32_t), "mb_result_t ABI mismatch");
_Static_assert(sizeof(mb_buffer_info_t) == (2u * sizeof(size_t)) + 8u, "mb_buffer_info_t size mismatch");
_Static_assert(offsetof(mb_buffer_info_t, length) == 0u, "mb_buffer_info_t.length offset mismatch");
_Static_assert(offsetof(mb_buffer_info_t, capacity) == sizeof(size_t), "mb_buffer_info_t.capacity offset mismatch");
_Static_assert(offsetof(mb_buffer_info_t, mapped) == 2u * sizeof(size_t), "mb_buffer_info_t.mapped offset mismatch");
#endif

/**
 * contextを初期化または再初期化する。context storageとarenaは重複できない。
 * 以前の初期化で得たhandle、map済みpointer、並行利用者が残っていないことを
 * 呼出側が保証する。
 *
 * @param context 呼出側が所有するopaqueな制御storage。
 * @param context_size MB_CONTEXT_SIZE以上を指定する。
 * @param arena このcontextが管理するデータmemory。
 * @param arena_length arenaのbyte数。
 * @return MB_OKまたはMB_INVALID_ARGUMENT。
 */
mb_result_t mb_init(
    mb_context_t *context,
    size_t context_size,
    uint8_t *arena,
    size_t arena_length);

/** 全handleを無効化する。MB_OK、MB_BUSY、context関連errorを返す。 */
mb_result_t mb_reset(mb_context_t *context);

/** capacity byteを割り当てる。MB_OK、MB_INVALID_ARGUMENT、MB_OUT_OF_MEMORY、
 * context関連errorを返す。 */
mb_result_t mb_alloc(
    mb_context_t *context,
    size_t capacity,
    mb_handle_t *out_handle);

/** handleを解放する。MB_OK、MB_BUSY、MB_INVALID_HANDLE、context関連errorを返す。 */
mb_result_t mb_free(mb_context_t *context, mb_handle_t handle);

/** 境界検査してバッファへcopyする。map中はMB_BUSYを返す。 */
mb_result_t mb_write(
    mb_context_t *context,
    mb_handle_t handle,
    size_t offset,
    const uint8_t *source,
    size_t length);

/** バッファの初期化済み範囲から境界検査してcopyする。map中はMB_BUSYを返す。 */
mb_result_t mb_read(
    mb_context_t *context,
    mb_handle_t handle,
    size_t offset,
    uint8_t *destination,
    size_t length);

/**
 * 割当capacityを変えずに論理長を設定する。
 * 論理長を伸ばす場合、新たな範囲はDMAまたはmap pointer経由で初期化済みで
 * なければならない。map中はMB_BUSYを返す。
 */
mb_result_t mb_set_length(
    mb_context_t *context,
    mb_handle_t handle,
    size_t length);

/** handleに対応する公開metadataを取得する。 */
mb_result_t mb_get_info(
    mb_context_t *context,
    mb_handle_t handle,
    mb_buffer_info_t *out_info);

/**
 * 返却pointerは対応するmb_unmapまで排他的に有効である。
 * 同じbufferへの二重mapはMB_BUSYを返す。
 * map中はmb_read、mb_write、mb_set_length、mb_freeを禁止する。
 * map pointer経由で書き込んだ場合は、unmap後にmb_set_lengthを呼ぶ。
 *
 * このAPIはDMAとzero-copy I/O用である。mb_unmapより前にdeviceまたはtaskが
 * pointerの利用を停止したことを呼出側が保証する。
 */
mb_result_t mb_map(
    mb_context_t *context,
    mb_handle_t handle,
    uint8_t **out_data,
    size_t *out_capacity);

/** 直接accessの貸出を1回分終了する。 */
mb_result_t mb_unmap(mb_context_t *context, mb_handle_t handle);

#ifdef __cplusplus
}
#endif

#endif
