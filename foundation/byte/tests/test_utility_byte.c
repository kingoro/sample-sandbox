/**
 * @file test_utility_byte.c
 * @brief Byte reader/writer単体テスト。
 */
#include "utility_byte.h"
#include "test_support.h"

#include <string.h>

/** @brief 全endian幅のround tripを検証する。 @return 成功時0。 */
static int test_round_trip(void)
{
    uint8_t data[64] = {0};
    ut_byte_writer_t writer;
    ut_byte_reader_t reader;
    uint8_t u8;
    uint16_t u16;
    uint32_t u32;
    uint64_t u64 = 0u;
    CHECK(ut_byte_writer_init(&writer, data, sizeof(data)) == UT_BYTE_OK);
    CHECK(ut_byte_write_u8(&writer, 0xabu) == UT_BYTE_OK);
    CHECK(ut_byte_write_be_u16(&writer, 0x1234u) == UT_BYTE_OK);
    CHECK(ut_byte_write_le_u16(&writer, 0x5678u) == UT_BYTE_OK);
    CHECK(ut_byte_write_be_u32(&writer, UINT32_C(0x12345678)) == UT_BYTE_OK);
    CHECK(ut_byte_write_le_u32(&writer, UINT32_C(0x90abcdef)) == UT_BYTE_OK);
    CHECK(ut_byte_write_be_u64(&writer, UINT64_C(0x0123456789abcdef)) == UT_BYTE_OK);
    CHECK(ut_byte_write_le_u64(&writer, UINT64_C(0xfedcba9876543210)) == UT_BYTE_OK);
    CHECK(ut_byte_writer_remaining(&writer) == sizeof(data) - writer.offset);

    CHECK(ut_byte_reader_init(&reader, data, writer.offset) == UT_BYTE_OK);
    CHECK(ut_byte_read_u8(&reader, &u8) == UT_BYTE_OK && u8 == 0xabu);
    CHECK(ut_byte_read_be_u16(&reader, &u16) == UT_BYTE_OK && u16 == 0x1234u);
    CHECK(ut_byte_read_le_u16(&reader, &u16) == UT_BYTE_OK && u16 == 0x5678u);
    CHECK(ut_byte_read_be_u32(&reader, &u32) == UT_BYTE_OK && u32 == UINT32_C(0x12345678));
    CHECK(ut_byte_read_le_u32(&reader, &u32) == UT_BYTE_OK && u32 == UINT32_C(0x90abcdef));
    CHECK(ut_byte_read_be_u64(&reader, &u64) == UT_BYTE_OK && u64 == UINT64_C(0x0123456789abcdef));
    CHECK(ut_byte_read_le_u64(&reader, &u64) == UT_BYTE_OK && u64 == UINT64_C(0xfedcba9876543210));
    CHECK(ut_byte_reader_remaining(&reader) == 0u);
    return 0;
}

/** @brief bounds失敗時にoffsetとbufferが不変であることを検証する。 @return 成功時0。 */
static int test_bounds_and_invalid_arguments(void)
{
    uint8_t data[2] = {0xaau, 0xbbu};
    uint8_t before[2];
    ut_byte_writer_t writer;
    ut_byte_reader_t reader;
    uint8_t u8;
    uint16_t u16;
    uint32_t value = 7u;
    uint64_t u64 = 0u;
    (void)memcpy(before, data, sizeof(data));
    CHECK(ut_byte_writer_init(&writer, data, sizeof(data)) == UT_BYTE_OK);
    CHECK(ut_byte_write_be_u32(&writer, value) == UT_BYTE_OUT_OF_BOUNDS);
    CHECK(writer.offset == 0u && memcmp(data, before, sizeof(data)) == 0);
    CHECK(ut_byte_reader_init(&reader, data, sizeof(data)) == UT_BYTE_OK);
    CHECK(ut_byte_read_be_u32(&reader, &value) == UT_BYTE_OUT_OF_BOUNDS);
    CHECK(reader.offset == 0u && value == 7u);
    CHECK(ut_byte_reader_init(NULL, data, 2u) == UT_BYTE_INVALID_ARGUMENT);
    CHECK(ut_byte_reader_init(&reader, NULL, 1u) == UT_BYTE_INVALID_ARGUMENT);
    CHECK(ut_byte_writer_init(NULL, data, 2u) == UT_BYTE_INVALID_ARGUMENT);
    CHECK(ut_byte_writer_init(&writer, NULL, 1u) == UT_BYTE_INVALID_ARGUMENT);
    CHECK(ut_byte_read_u8(NULL, (uint8_t *)&value) == UT_BYTE_INVALID_ARGUMENT);
    CHECK(ut_byte_read_u8(&reader, NULL) == UT_BYTE_INVALID_ARGUMENT);
    CHECK(ut_byte_read_be_u16(&reader, NULL) == UT_BYTE_INVALID_ARGUMENT);
    CHECK(ut_byte_read_le_u16(&reader, NULL) == UT_BYTE_INVALID_ARGUMENT);
    CHECK(ut_byte_read_be_u32(&reader, NULL) == UT_BYTE_INVALID_ARGUMENT);
    CHECK(ut_byte_read_le_u32(&reader, NULL) == UT_BYTE_INVALID_ARGUMENT);
    CHECK(ut_byte_read_be_u64(&reader, NULL) == UT_BYTE_INVALID_ARGUMENT);
    CHECK(ut_byte_read_le_u64(&reader, NULL) == UT_BYTE_INVALID_ARGUMENT);

    reader.data = NULL;
    reader.size = 1u;
    reader.offset = 0u;
    CHECK(ut_byte_read_u8(&reader, &u8) == UT_BYTE_INVALID_ARGUMENT);
    CHECK(ut_byte_reader_remaining(&reader) == 0u);
    reader.data = data;
    reader.size = 1u;
    reader.offset = 2u;
    CHECK(ut_byte_read_be_u16(&reader, &u16) == UT_BYTE_INVALID_ARGUMENT);
    CHECK(ut_byte_reader_remaining(&reader) == 0u);

    writer.data = NULL;
    writer.size = 1u;
    writer.offset = 0u;
    CHECK(ut_byte_write_u8(&writer, 1u) == UT_BYTE_INVALID_ARGUMENT);
    CHECK(ut_byte_writer_remaining(&writer) == 0u);
    writer.data = data;
    writer.size = 1u;
    writer.offset = 2u;
    CHECK(ut_byte_write_be_u64(&writer, u64) == UT_BYTE_INVALID_ARGUMENT);
    CHECK(ut_byte_writer_remaining(&writer) == 0u);
    CHECK(ut_byte_write_u8(NULL, 1u) == UT_BYTE_INVALID_ARGUMENT);

    CHECK(ut_byte_reader_init(&reader, NULL, 0u) == UT_BYTE_OK);
    CHECK(ut_byte_writer_init(&writer, NULL, 0u) == UT_BYTE_OK);
    CHECK(ut_byte_reader_remaining(NULL) == 0u);
    CHECK(ut_byte_writer_remaining(NULL) == 0u);
    reader.offset = 1u;
    writer.offset = 1u;
    CHECK(ut_byte_reader_remaining(&reader) == 0u);
    CHECK(ut_byte_writer_remaining(&writer) == 0u);
    return 0;
}

/**
 * Byte Foundation単体テストを実行する。
 *
 * @return 全成功時0、失敗時1。
 */
int main(void)
{
    CHECK(test_round_trip() == 0);
    CHECK(test_bounds_and_invalid_arguments() == 0);
    return 0;
}
