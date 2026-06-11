/**
 * @file utility_byte_writer.h
 * @brief Bounds付きbyte writer API。
 */
#ifndef UTILITY_BYTE_WRITER_H
#define UTILITY_BYTE_WRITER_H

#include "utility_byte_result.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 呼出側所有bufferへ順次書くwriter。 */
typedef struct ut_byte_writer {
    /** 書込先buffer。writer利用中は呼出側が有効に保つ。 */
    uint8_t *data;
    /** dataのbyte数。 */
    size_t size;
    /** 次に書く位置。成功した書込時だけ進む。 */
    size_t offset;
} ut_byte_writer_t;

/**
 * Writerを初期化する。
 *
 * heapを使わない。writerはthread safeではなく、外部同期なしに共有できない。
 *
 * @param writer 初期化するwriter。
 * @param data 書込先buffer。sizeが0の場合のみNULL可。
 * @param size dataのbyte数。
 * @return UT_BYTE_OKまたはUT_BYTE_INVALID_ARGUMENT。
 */
ut_byte_result_t ut_byte_writer_init(
    ut_byte_writer_t *writer,
    void *data,
    size_t size);

/**
 * Writerの未使用byte数を返す。
 *
 * @param writer 初期化済みwriter。
 * @return 未使用byte数。不正なwriterでは0。
 */
size_t ut_byte_writer_remaining(const ut_byte_writer_t *writer);

/** @brief u8を書く。 @param writer writer。 @param value 値。 @return result code。 */
ut_byte_result_t ut_byte_write_u8(ut_byte_writer_t *writer, uint8_t value);
/** @brief BE u16を書く。 @param writer writer。 @param value 値。 @return result code。 */
ut_byte_result_t ut_byte_write_be_u16(ut_byte_writer_t *writer, uint16_t value);
/** @brief LE u16を書く。 @param writer writer。 @param value 値。 @return result code。 */
ut_byte_result_t ut_byte_write_le_u16(ut_byte_writer_t *writer, uint16_t value);
/** @brief BE u32を書く。 @param writer writer。 @param value 値。 @return result code。 */
ut_byte_result_t ut_byte_write_be_u32(ut_byte_writer_t *writer, uint32_t value);
/** @brief LE u32を書く。 @param writer writer。 @param value 値。 @return result code。 */
ut_byte_result_t ut_byte_write_le_u32(ut_byte_writer_t *writer, uint32_t value);
/** @brief BE u64を書く。 @param writer writer。 @param value 値。 @return result code。 */
ut_byte_result_t ut_byte_write_be_u64(ut_byte_writer_t *writer, uint64_t value);
/** @brief LE u64を書く。 @param writer writer。 @param value 値。 @return result code。 */
ut_byte_result_t ut_byte_write_le_u64(ut_byte_writer_t *writer, uint64_t value);

#ifdef __cplusplus
}
#endif

#endif
