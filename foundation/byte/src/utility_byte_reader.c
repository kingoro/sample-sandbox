/**
 * @file utility_byte_reader.c
 * @brief Bounds付きbyte reader実装。
 */
#include "utility_byte_reader.h"

#include <stdbool.h>

/**
 * 指定幅の整数を読み、endianに従ってuint64へ展開する。
 *
 * @param reader 読取元reader。
 * @param width 読むbyte数。
 * @param little_endian LEの場合true。
 * @param value 展開値の出力先。
 * @return 成功、引数不正、またはbounds error。
 */
static ut_byte_result_t read_integer(
    ut_byte_reader_t *reader,
    size_t width,
    bool little_endian,
    uint64_t *value)
{
    size_t index;
    uint64_t result = 0u;
    if (reader == NULL ||
        (reader->data == NULL && reader->size != 0u) ||
        reader->offset > reader->size) {
        return UT_BYTE_INVALID_ARGUMENT;
    }
    if (width > reader->size - reader->offset) {
        return UT_BYTE_OUT_OF_BOUNDS;
    }
    for (index = 0u; index < width; ++index) {
        size_t source = little_endian ? width - index - 1u : index;
        result = (result << 8u) | reader->data[reader->offset + source];
    }
    reader->offset += width;
    *value = result;
    return UT_BYTE_OK;
}

ut_byte_result_t ut_byte_reader_init(
    ut_byte_reader_t *reader,
    const void *data,
    size_t size)
{
    if (reader == NULL || (data == NULL && size != 0u)) {
        return UT_BYTE_INVALID_ARGUMENT;
    }
    reader->data = data;
    reader->size = size;
    reader->offset = 0u;
    return UT_BYTE_OK;
}

size_t ut_byte_reader_remaining(const ut_byte_reader_t *reader)
{
    if (reader == NULL || reader->offset > reader->size ||
        (reader->data == NULL && reader->size != 0u)) {
        return 0u;
    }
    return reader->size - reader->offset;
}

ut_byte_result_t ut_byte_read_u8(ut_byte_reader_t *reader, uint8_t *value)
{
    uint64_t result_value;
    ut_byte_result_t result;
    if (value == NULL) {
        return UT_BYTE_INVALID_ARGUMENT;
    }
    result = read_integer(reader, 1u, false, &result_value);
    if (result == UT_BYTE_OK) {
        *value = (uint8_t)result_value;
    }
    return result;
}

/**
 * 型別read wrapperの共通処理。
 *
 * @param reader 読取元reader。
 * @param width 読むbyte数。
 * @param little_endian LEの場合true。
 * @param value uint64出力先。
 * @return 成功、引数不正、またはbounds error。
 */
static ut_byte_result_t read_checked(
    ut_byte_reader_t *reader,
    size_t width,
    bool little_endian,
    uint64_t *value)
{
    if (value == NULL) {
        return UT_BYTE_INVALID_ARGUMENT;
    }
    return read_integer(reader, width, little_endian, value);
}

ut_byte_result_t ut_byte_read_be_u16(ut_byte_reader_t *reader, uint16_t *value)
{
    uint64_t result;
    ut_byte_result_t status = read_checked(reader, 2u, false, value == NULL ? NULL : &result);
    if (status == UT_BYTE_OK) {
        *value = (uint16_t)result;
    }
    return status;
}

ut_byte_result_t ut_byte_read_le_u16(ut_byte_reader_t *reader, uint16_t *value)
{
    uint64_t result;
    ut_byte_result_t status = read_checked(reader, 2u, true, value == NULL ? NULL : &result);
    if (status == UT_BYTE_OK) {
        *value = (uint16_t)result;
    }
    return status;
}

ut_byte_result_t ut_byte_read_be_u32(ut_byte_reader_t *reader, uint32_t *value)
{
    uint64_t result;
    ut_byte_result_t status = read_checked(reader, 4u, false, value == NULL ? NULL : &result);
    if (status == UT_BYTE_OK) {
        *value = (uint32_t)result;
    }
    return status;
}

ut_byte_result_t ut_byte_read_le_u32(ut_byte_reader_t *reader, uint32_t *value)
{
    uint64_t result;
    ut_byte_result_t status = read_checked(reader, 4u, true, value == NULL ? NULL : &result);
    if (status == UT_BYTE_OK) {
        *value = (uint32_t)result;
    }
    return status;
}

ut_byte_result_t ut_byte_read_be_u64(ut_byte_reader_t *reader, uint64_t *value)
{
    return read_checked(reader, 8u, false, value);
}

ut_byte_result_t ut_byte_read_le_u64(ut_byte_reader_t *reader, uint64_t *value)
{
    return read_checked(reader, 8u, true, value);
}
