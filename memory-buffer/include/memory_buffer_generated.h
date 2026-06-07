/**
 * @file memory_buffer_generated.h
 * @brief Rust定義から生成したMemory Buffer ABI manifest。
 *
 * 自動生成ファイル。直接編集しないこと。
 * 利用者向けの正式APIはmemory_buffer.hを参照する。
 */


#ifndef MEMORY_BUFFER_GENERATED_H
#define MEMORY_BUFFER_GENERATED_H

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

/**
 * C側で確保する`mb_context_t`に必要なalignment。
 */
#define MB_CONTEXT_ALIGNMENT 8

/**
 * C側で確保する`mb_context_t`のbyte数。
 */
#define MB_CONTEXT_SIZE 4096

/**
 * 1 contextで同時に管理できる最大バッファ数。
 */
#define MB_MAX_BUFFERS 64

/**
 * すべてのC ABI操作が返す結果code。
 */
enum mb_result_generated_t
#if defined(__cplusplus) || __STDC_VERSION__ >= 202311L
  : uint32_t
#endif // defined(__cplusplus) || __STDC_VERSION__ >= 202311L
 {
  /**
   * 操作が成功した。
   */
  Ok = 0,
  /**
   * NULL、size、alignmentなどの引数契約に違反した。
   */
  InvalidArgument = 1,
  /**
   * contextが初期化されていない。
   */
  NotInitialized = 2,
  /**
   * arenaまたはbuffer slotに空きがない。
   */
  OutOfMemory = 4,
  /**
   * handleが無効、解放済み、または世代不一致である。
   */
  InvalidHandle = 5,
  /**
   * read、write、lengthがbuffer境界を超える。
   */
  OutOfBounds = 6,
  /**
   * map中など、現在の状態では操作できない。
   */
  Busy = 7,
};
#ifndef __cplusplus
#if __STDC_VERSION__ >= 202311L
typedef enum mb_result_generated_t mb_result_generated_t;
#else
typedef uint32_t mb_result_generated_t;
#endif // __STDC_VERSION__ >= 202311L
#endif // __cplusplus

/**
 * 割り当て済みバッファの公開状態。
 */
struct mb_buffer_info_generated_t {
  /**
   * 初期化済みデータの論理長。
   */
  size_t length;
  /**
   * bufferへ割り当てられた最大byte数。
   */
  size_t capacity;
  /**
   * map中なら1、それ以外は0。
   */
  uint8_t mapped;
  /**
   * ABI拡張用。常に0として返す。
   */
  uint8_t reserved[7];
};

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

/**
 * 領域を割り当て、世代付きhandleを返す。
 *
 * # Safety
 *
 * `storage`は初期化済み、`out_handle`は書込み可能でなければならない。
 * accessは呼出側で直列化しなければならない。
 *
 * @param storage 初期化済み制御storage。
 * @param capacity 確保するbyte数。
 * @param out_handle 成功時にhandleを格納する出力先。
 * @return 操作結果。
 */
mb_result_generated_t mb_alloc(void *storage, size_t capacity, uint32_t *out_handle);

/**
 * map貸出中でなければバッファを解放する。
 *
 * # Safety
 *
 * `storage`には初期化済みcontextが必要で、accessは呼出側で直列化する。
 *
 * @param storage 初期化済み制御storage。
 * @param handle 解放対象handle。
 * @return 操作結果。
 */
mb_result_generated_t mb_free(void *storage, uint32_t handle);

/**
 * バッファの論理長、capacity、map状態を返す。
 *
 * # Safety
 *
 * `out_info`は書込み可能でなければならない。`storage`には初期化済みcontextが
 * 必要で、accessは呼出側で直列化する。
 *
 * @param storage 初期化済み制御storage。
 * @param handle 対象handle。
 * @param out_info metadataの格納先。
 * @return 操作結果。
 */
mb_result_generated_t mb_get_info(void *storage,
                                  uint32_t handle,
                                  struct mb_buffer_info_generated_t *out_info);

/**
 * 制御storageを初期化し、呼出側所有のデータarenaへ関連付ける。
 *
 * # Safety
 *
 * すべてのpointerは、指定したsizeだけ有効なmemoryを参照しなければならない。
 * control storageとarenaは重複してはならない。
 * contextとarenaへのaccessは呼出側で直列化しなければならない。
 *
 * @param storage C側が確保した制御storage。
 * @param storage_size storageのbyte数。
 * @param arena payloadを保持する呼出側所有memory。
 * @param arena_len arenaのbyte数。
 * @return 操作結果。
 */
mb_result_generated_t mb_init(void *storage,
                              size_t storage_size,
                              uint8_t *arena,
                              size_t arena_len);

/**
 * 対応する[`mb_unmap`]まで、arenaへの排他的な直接accessを一時貸出する。
 *
 * # Safety
 *
 * 出力pointerは書込み可能でなければならない。呼出側は返却されたpointerの
 * 貸出期間を守り、context accessを直列化しなければならない。
 *
 * @param storage 初期化済み制御storage。
 * @param handle map対象handle。
 * @param out_data arena内pointerの格納先。
 * @param out_capacity capacityの格納先。
 * @return 操作結果。
 */
mb_result_generated_t mb_map(void *storage,
                             uint32_t handle,
                             uint8_t **out_data,
                             size_t *out_capacity);

/**
 * 初期化済みbyte列をバッファから呼出側所有memoryへcopyする。
 *
 * # Safety
 *
 * `destination`は`length` byte書込み可能でなければならない。`storage`には
 * 初期化済みcontextが必要で、accessは呼出側で直列化する。
 *
 * @param storage 初期化済み制御storage。
 * @param handle 読取り対象handle。
 * @param offset buffer先頭からのoffset。
 * @param destination copy先byte列。
 * @param length copyするbyte数。
 * @return 操作結果。
 */
mb_result_generated_t mb_read(void *storage,
                              uint32_t handle,
                              size_t offset,
                              uint8_t *destination,
                              size_t length);

/**
 * map中のバッファがないことを確認し、すべてのhandleを無効化する。
 *
 * # Safety
 *
 * `storage`には[`mb_init`]で初期化したcontextが必要であり、accessは呼出側で
 * 直列化しなければならない。
 *
 * @param storage 初期化済み制御storage。
 * @return 操作結果。
 */
mb_result_generated_t mb_reset(void *storage);

/**
 * capacityを変更せず論理長だけを変更する。
 *
 * # Safety
 *
 * `storage`には初期化済みcontextが必要で、accessは呼出側で直列化する。
 * 論理長を伸ばす場合、新たに有効化する範囲はmap pointerなどで
 * 初期化済みでなければならない。
 *
 * @param storage 初期化済み制御storage。
 * @param handle 対象handle。
 * @param length 設定する論理長。
 * @return 操作結果。
 */
mb_result_generated_t mb_set_length(void *storage, uint32_t handle, size_t length);

/**
 * [`mb_map`]で開始した直接access貸出を1回分終了する。
 *
 * # Safety
 *
 * `storage`には初期化済みcontextが必要で、accessは呼出側で直列化する。
 * 最後のunmapより前に、mapしたpointerの利用者をすべて停止しなければならない。
 *
 * @param storage 初期化済み制御storage。
 * @param handle unmap対象handle。
 * @return 操作結果。
 */
mb_result_generated_t mb_unmap(void *storage,
                               uint32_t handle);

/**
 * バッファへbyte列をcopyし、必要に応じて論理長を伸ばす。
 *
 * # Safety
 *
 * `source`は`length` byte読取り可能でなければならない。`storage`には
 * 初期化済みcontextが必要で、accessは呼出側で直列化する。
 *
 * @param storage 初期化済み制御storage。
 * @param handle 書込み対象handle。
 * @param offset buffer先頭からのoffset。
 * @param source copy元byte列。
 * @param length copyするbyte数。
 * @return 操作結果。
 */
mb_result_generated_t mb_write(void *storage,
                               uint32_t handle,
                               size_t offset,
                               const uint8_t *source,
                               size_t length);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  /* MEMORY_BUFFER_GENERATED_H */
