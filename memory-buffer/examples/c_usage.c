/**
 * @file c_usage.c
 * @brief Memory Buffer公開C APIの結合テスト兼利用例。
 */
#include "memory_buffer.h"

#include <assert.h>
#include <string.h>

/** 結合テストで使用するMemory Buffer context。 */
static mb_context_t context;
/** 結合テストでpayloadを保持する固定長arena。 */
static uint8_t arena[64u * 1024u];

/**
 * C applicationから公開headerとRust static libraryを利用できることを確認する
 * 最小結合テスト。基本的なcopy lifecycleとmetadata更新を検証する。
 *
 * @return 全検証成功時は0。assert違反時はprocessが異常終了する。
 */
int main(void)
{
    mb_handle_t handle;
    mb_handle_t mapped_handle;
    const uint8_t source[] = {0x10, 0x20, 0x30, 0x40};
    uint8_t destination[sizeof(source)] = {0};
    uint8_t *mapped_data;
    size_t mapped_capacity;
    mb_buffer_info_t info;

    assert(MB_ABI_VERSION == ((1u << 16u) | 0u));

    /* 初期化し、payloadを保持するバッファを確保する。 */
    assert(mb_init(&context, sizeof(context), arena, sizeof(arena)) == MB_OK);
    assert(mb_alloc(&context, 1024u, &handle) == MB_OK);

    /* copy APIを通して、データ本体と論理長が正しく反映されることを確認する。 */
    assert(mb_write(&context, handle, 0u, source, sizeof(source)) == MB_OK);
    assert(mb_get_info(&context, handle, &info) == MB_OK);
    assert(info.length == sizeof(source));
    assert(mb_read(
        &context, handle, 0u, destination, sizeof(destination)) == MB_OK);
    assert(memcmp(source, destination, sizeof(source)) == 0);

    /* lifecycleの終端で所有権を返却する。 */
    assert(mb_free(&context, handle) == MB_OK);

    /*
     * map pointerを使う直接access経路と、返却後の論理長更新を確認する。
     */
    assert(mb_alloc(&context, 16u, &mapped_handle) == MB_OK);
    assert(mb_map(
        &context, mapped_handle, &mapped_data, &mapped_capacity) == MB_OK);
    assert(mapped_capacity == 16u);
    mapped_data[0] = 0x55u;
    assert(mb_unmap(&context, mapped_handle) == MB_OK);
    assert(mb_set_length(&context, mapped_handle, 1u) == MB_OK);
    assert(mb_read(
        &context, mapped_handle, 0u, destination, 1u) == MB_OK);
    assert(destination[0] == 0x55u);

    /* resetが残存handleを無効化することを確認する。 */
    assert(mb_reset(&context) == MB_OK);
    assert(mb_free(&context, mapped_handle) == MB_INVALID_HANDLE);

    return 0;
}
