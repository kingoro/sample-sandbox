/**
 * @file utility_byte_reader.h
 * @brief Bounds付きbyte reader API。
 */
#ifndef UTILITY_BYTE_READER_H
#define UTILITY_BYTE_READER_H

#include "utility_byte_result.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 呼出側所有bufferを順次読むreader。 */
typedef struct ut_byte_reader {
    /** 読取元buffer。reader利用中は呼出側が有効に保つ。 */
    const uint8_t *data;
    /** dataのbyte数。 */
    size_t size;
    /** 次に読む位置。成功した読取時だけ進む。 */
    size_t offset;
} ut_byte_reader_t;

/**
 * Readerを初期化する。
 *
 * heapを使わない。readerはthread safeではなく、外部同期なしに共有できない。
 *
 * @param reader 初期化するreader。
 * @param data 読取元buffer。sizeが0の場合のみNULL可。
 * @param size dataのbyte数。
 * @return UT_BYTE_OKまたはUT_BYTE_INVALID_ARGUMENT。
 */
ut_byte_result_t ut_byte_reader_init(
    ut_byte_reader_t *reader,
    const void *data,
    size_t size);

/**
 * Readerの未読byte数を返す。
 *
 * @param reader 初期化済みreader。
 * @return 未読byte数。不正なreaderでは0。
 */
size_t ut_byte_reader_remaining(const ut_byte_reader_t *reader);

/** @brief u8を読む。 @param reader reader。 @param value 出力先。 @return result code。 */
ut_byte_result_t ut_byte_read_u8(ut_byte_reader_t *reader, uint8_t *value);
/** @brief BE u16を読む。 @param reader reader。 @param value 出力先。 @return result code。 */
ut_byte_result_t ut_byte_read_be_u16(ut_byte_reader_t *reader, uint16_t *value);
/** @brief LE u16を読む。 @param reader reader。 @param value 出力先。 @return result code。 */
ut_byte_result_t ut_byte_read_le_u16(ut_byte_reader_t *reader, uint16_t *value);
/** @brief BE u32を読む。 @param reader reader。 @param value 出力先。 @return result code。 */
ut_byte_result_t ut_byte_read_be_u32(ut_byte_reader_t *reader, uint32_t *value);
/** @brief LE u32を読む。 @param reader reader。 @param value 出力先。 @return result code。 */
ut_byte_result_t ut_byte_read_le_u32(ut_byte_reader_t *reader, uint32_t *value);
/** @brief BE u64を読む。 @param reader reader。 @param value 出力先。 @return result code。 */
ut_byte_result_t ut_byte_read_be_u64(ut_byte_reader_t *reader, uint64_t *value);
/** @brief LE u64を読む。 @param reader reader。 @param value 出力先。 @return result code。 */
ut_byte_result_t ut_byte_read_le_u64(ut_byte_reader_t *reader, uint64_t *value);

#ifdef __cplusplus
}
#endif

#endif
