/**
 * @file utility_byte_writer.c
 * @brief Bounds付きbyte writer実装。
 */
#include "utility_byte_writer.h"

#include <stdbool.h>

/**
 * 指定幅の整数をendianに従って書く。
 *
 * @param writer 書込先writer。
 * @param value 書く値。
 * @param width 書くbyte数。
 * @param little_endian LEの場合true。
 * @return 成功、引数不正、またはbounds error。
 */
static ut_byte_result_t write_integer(
    ut_byte_writer_t *writer,
    uint64_t value,
    size_t width,
    bool little_endian)
{
    size_t index;
    if (writer == NULL ||
        (writer->data == NULL && writer->size != 0u) ||
        writer->offset > writer->size) {
        return UT_BYTE_INVALID_ARGUMENT;
    }
    if (width > writer->size - writer->offset) {
        return UT_BYTE_OUT_OF_BOUNDS;
    }
    for (index = 0u; index < width; ++index) {
        size_t target = little_endian ? index : width - index - 1u;
        writer->data[writer->offset + target] = (uint8_t)value;
        value >>= 8u;
    }
    writer->offset += width;
    return UT_BYTE_OK;
}

ut_byte_result_t ut_byte_writer_init(
    ut_byte_writer_t *writer,
    void *data,
    size_t size)
{
    if (writer == NULL || (data == NULL && size != 0u)) {
        return UT_BYTE_INVALID_ARGUMENT;
    }
    writer->data = data;
    writer->size = size;
    writer->offset = 0u;
    return UT_BYTE_OK;
}

size_t ut_byte_writer_remaining(const ut_byte_writer_t *writer)
{
    if (writer == NULL || writer->offset > writer->size ||
        (writer->data == NULL && writer->size != 0u)) {
        return 0u;
    }
    return writer->size - writer->offset;
}

ut_byte_result_t ut_byte_write_u8(ut_byte_writer_t *writer, uint8_t value)
{
    return write_integer(writer, value, 1u, false);
}

ut_byte_result_t ut_byte_write_be_u16(ut_byte_writer_t *writer, uint16_t value)
{
    return write_integer(writer, value, 2u, false);
}

ut_byte_result_t ut_byte_write_le_u16(ut_byte_writer_t *writer, uint16_t value)
{
    return write_integer(writer, value, 2u, true);
}

ut_byte_result_t ut_byte_write_be_u32(ut_byte_writer_t *writer, uint32_t value)
{
    return write_integer(writer, value, 4u, false);
}

ut_byte_result_t ut_byte_write_le_u32(ut_byte_writer_t *writer, uint32_t value)
{
    return write_integer(writer, value, 4u, true);
}

ut_byte_result_t ut_byte_write_be_u64(ut_byte_writer_t *writer, uint64_t value)
{
    return write_integer(writer, value, 8u, false);
}

ut_byte_result_t ut_byte_write_le_u64(ut_byte_writer_t *writer, uint64_t value)
{
    return write_integer(writer, value, 8u, true);
}
