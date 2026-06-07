/**
 * @file memory_buffer.h
 * @brief 呼出側提供arenaを管理するMemory Bufferの正式C API。
 */
#ifndef MEMORY_BUFFER_H
#define MEMORY_BUFFER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** mb_context_tの必要storage byte数。 */
#define MB_CONTEXT_SIZE 4096u
/** mb_context_tに必要なalignment。 */
#define MB_CONTEXT_ALIGNMENT 8u
/** 1 contextで同時に管理できる最大buffer数。 */
#define MB_MAX_BUFFERS 64u
/** C ABIのmajor version。 */
#define MB_ABI_VERSION_MAJOR 1u
/** C ABIのminor version。 */
#define MB_ABI_VERSION_MINOR 0u
/** majorとminorを結合したC ABI version。 */
#define MB_ABI_VERSION ((MB_ABI_VERSION_MAJOR << 16u) | MB_ABI_VERSION_MINOR)

/** 世代番号付きtoken。bit layoutはlibrary内部仕様とする。 */
typedef uint32_t mb_handle_t;

/** すべてのmemory-buffer操作が返す32bit状態code。 */
typedef uint32_t mb_result_t;

enum {
    /** 操作が成功した。 */
    MB_OK = 0,
    /** NULL、size、alignmentなどの引数契約に違反した。 */
    MB_INVALID_ARGUMENT = 1,
    /** contextがmb_initで初期化されていない。 */
    MB_NOT_INITIALIZED = 2,
    /** arenaまたはbuffer slotに空きがない。 */
    MB_OUT_OF_MEMORY = 4,
    /** handleが無効、解放済み、または世代不一致である。 */
    MB_INVALID_HANDLE = 5,
    /** read、write、lengthがbuffer境界を超える。 */
    MB_OUT_OF_BOUNDS = 6,
    /** map中など、現在の状態では操作できない。 */
    MB_BUSY = 7
};

/** 静的確保できるopaqueな制御storage。 */
typedef union mb_context {
    /** union全体へ必要alignmentを与えるためのmember。 */
    uint64_t _alignment;
    /** Memory Buffer内部contextを格納するopaque storage。 */
    uint8_t _storage[MB_CONTEXT_SIZE];
} mb_context_t;

/** 割り当て済みバッファの公開metadata。 */
typedef struct mb_buffer_info {
    /** 初期化済みデータの論理長。 */
    size_t length;
    /** bufferへ割り当てられた最大byte数。 */
    size_t capacity;
    /** map中なら1、それ以外は0。 */
    uint8_t mapped;
    /** ABI拡張用。常に0として返す。 */
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

/**
 * 全handleを無効化する。
 *
 * @param context 初期化済みcontext。
 * @return MB_OK、MB_BUSY、またはcontext関連error。
 */
mb_result_t mb_reset(mb_context_t *context);

/**
 * capacity byteを割り当てる。
 *
 * @param context 初期化済みcontext。
 * @param capacity 確保するbyte数。0は指定できない。
 * @param out_handle 成功時に世代付きhandleを格納する出力先。
 * @return MB_OK、MB_INVALID_ARGUMENT、MB_OUT_OF_MEMORY、またはcontext関連error。
 */
mb_result_t mb_alloc(
    mb_context_t *context,
    size_t capacity,
    mb_handle_t *out_handle);

/**
 * handleを解放する。
 *
 * @param context 初期化済みcontext。
 * @param handle 解放対象handle。
 * @return MB_OK、MB_BUSY、MB_INVALID_HANDLE、またはcontext関連error。
 */
mb_result_t mb_free(mb_context_t *context, mb_handle_t handle);

/**
 * 境界検査してバッファへcopyする。map中はMB_BUSYを返す。
 *
 * @param context 初期化済みcontext。
 * @param handle 書込み対象handle。
 * @param offset buffer先頭からの書込みoffset。
 * @param source copy元byte列。lengthが0ならNULLでもよい。
 * @param length copyするbyte数。
 * @return MB_OKまたは引数、handle、状態、境界に対応するerror。
 */
mb_result_t mb_write(
    mb_context_t *context,
    mb_handle_t handle,
    size_t offset,
    const uint8_t *source,
    size_t length);

/**
 * バッファの初期化済み範囲から境界検査してcopyする。
 *
 * @param context 初期化済みcontext。
 * @param handle 読取り対象handle。
 * @param offset buffer先頭からの読取りoffset。
 * @param destination copy先byte列。lengthが0ならNULLでもよい。
 * @param length copyするbyte数。
 * @return MB_OKまたは引数、handle、状態、境界に対応するerror。
 */
mb_result_t mb_read(
    mb_context_t *context,
    mb_handle_t handle,
    size_t offset,
    uint8_t *destination,
    size_t length);

/**
 * 割当capacityを変えずに論理長を設定する。
 * 論理長を伸ばす場合、新たな範囲はmap pointerなどで初期化済みで
 * なければならない。map中はMB_BUSYを返す。
 *
 * @param context 初期化済みcontext。
 * @param handle 対象handle。
 * @param length 設定する論理長。
 * @return MB_OKまたはhandle、状態、境界に対応するerror。
 */
mb_result_t mb_set_length(
    mb_context_t *context,
    mb_handle_t handle,
    size_t length);

/**
 * handleに対応する公開metadataを取得する。
 *
 * @param context 初期化済みcontext。
 * @param handle 対象handle。
 * @param out_info metadataの格納先。
 * @return MB_OKまたは引数、handle、contextに対応するerror。
 */
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
 * このAPIはcopyを介さない直接access用である。mb_unmapより前に、非同期処理を
 * 含む全利用者がpointerの利用を停止したことを呼出側が保証する。
 *
 * @param context 初期化済みcontext。
 * @param handle map対象handle。
 * @param out_data 成功時にarena内pointerを格納する出力先。
 * @param out_capacity 成功時に割当capacityを格納する出力先。
 * @return MB_OKまたは引数、handle、状態に対応するerror。
 */
mb_result_t mb_map(
    mb_context_t *context,
    mb_handle_t handle,
    uint8_t **out_data,
    size_t *out_capacity);

/**
 * 直接accessの貸出を1回分終了する。
 *
 * @param context 初期化済みcontext。
 * @param handle unmap対象handle。
 * @return MB_OKまたは引数、handle、contextに対応するerror。
 */
mb_result_t mb_unmap(mb_context_t *context, mb_handle_t handle);

#ifdef __cplusplus
}
#endif

#endif
