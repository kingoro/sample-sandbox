/**
 * @file utility_id.h
 * @brief uint32 request ID generator API。
 */
#ifndef UTILITY_ID_H
#define UTILITY_ID_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Request ID generator。
 *
 * 単一thread専用であり、同時利用には呼出側同期が必要。0は予約値で生成しない。
 * 使用中IDとの衝突を完全回避する責務は持たない。
 */
typedef struct ut_id_generator {
    /** 最後に生成したID。0は未生成状態。 */
    uint32_t current;
} ut_id_generator_t;

/**
 * Generatorを初期化する。
 *
 * @param generator 初期化する呼出側所有generator。
 * @param current 最後に使用済みとみなす値。0なら最初のIDは1。
 */
void ut_id_init(ut_id_generator_t *generator, uint32_t current);

/**
 * 次のrequest IDを返す。
 *
 * UINT32_MAXの次は1へ戻る。generatorは単一threadで利用する。
 *
 * @param generator 初期化済みgenerator。
 * @return 次の非0 ID。NULLの場合は予約値0。
 */
uint32_t ut_id_next(ut_id_generator_t *generator);

#ifdef __cplusplus
}
#endif

#endif
