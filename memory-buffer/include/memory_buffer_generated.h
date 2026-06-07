/* 自動生成ファイル。直接編集しないこと。 */


#ifndef MEMORY_BUFFER_GENERATED_H
#define MEMORY_BUFFER_GENERATED_H

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#define MB_CONTEXT_ALIGNMENT 8

#define MB_CONTEXT_SIZE 4096

#define MB_MAX_BUFFERS 64

enum mb_result_generated_t
#if defined(__cplusplus) || __STDC_VERSION__ >= 202311L
  : uint32_t
#endif // defined(__cplusplus) || __STDC_VERSION__ >= 202311L
 {
  Ok = 0,
  InvalidArgument = 1,
  NotInitialized = 2,
  OutOfMemory = 4,
  InvalidHandle = 5,
  OutOfBounds = 6,
  Busy = 7,
};
#ifndef __cplusplus
#if __STDC_VERSION__ >= 202311L
typedef enum mb_result_generated_t mb_result_generated_t;
#else
typedef uint32_t mb_result_generated_t;
#endif // __STDC_VERSION__ >= 202311L
#endif // __cplusplus

struct mb_buffer_info_generated_t {
  size_t length;
  size_t capacity;
  uint8_t mapped;
  uint8_t reserved[7];
};

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

mb_result_generated_t mb_alloc(void *storage, size_t capacity, uint32_t *out_handle);

mb_result_generated_t mb_free(void *storage, uint32_t handle);

mb_result_generated_t mb_get_info(void *storage,
                                  uint32_t handle,
                                  struct mb_buffer_info_generated_t *out_info);

mb_result_generated_t mb_init(void *storage, size_t storage_size, uint8_t *arena, size_t arena_len);

mb_result_generated_t mb_map(void *storage,
                             uint32_t handle,
                             uint8_t **out_data,
                             size_t *out_capacity);

mb_result_generated_t mb_read(void *storage,
                              uint32_t handle,
                              size_t offset,
                              uint8_t *destination,
                              size_t length);

mb_result_generated_t mb_reset(void *storage);

mb_result_generated_t mb_set_length(void *storage, uint32_t handle, size_t length);

mb_result_generated_t mb_unmap(void *storage, uint32_t handle);

mb_result_generated_t mb_write(void *storage,
                               uint32_t handle,
                               size_t offset,
                               const uint8_t *source,
                               size_t length);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  /* MEMORY_BUFFER_GENERATED_H */
