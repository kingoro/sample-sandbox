#ifndef UNIT_D_H
#define UNIT_D_H

#include <stdbool.h>

/**
 * @file unit_d.h
 * @brief Unit Dの公開インターフェース。
 *
 * Unit Dは、`domain` 層のStepから呼び出される部品操作モジュールのサンプルです。
 * 関数名は「動詞_対象」の順番にして、何をする関数かを先頭で読み取れるようにしています。
 * 現時点では実機操作は行わず、各APIが呼ばれたことを標準出力へ表示します。
 */

/**
 * @brief Unit Dの状態。
 */
typedef enum {
    /** 初期化されていない状態。 */
    UNIT_D_STATUS_NOT_INITIALIZED = 0,
    /** 初期化済みで操作を受け付けられる状態。 */
    UNIT_D_STATUS_READY,
    /** open操作後の状態。 */
    UNIT_D_STATUS_OPEN,
    /** move操作後の状態。 */
    UNIT_D_STATUS_MOVING,
    /** close操作後の状態。 */
    UNIT_D_STATUS_CLOSED
} unit_d_status_t;

/**
 * @brief Unit Dを初期化する。
 *
 * @return 初期化できた場合は `true`、失敗した場合は `false`。
 */
bool initialize_unit_d(void);

/**
 * @brief Unit Dを終了する。
 *
 * @return 終了できた場合は `true`、失敗した場合は `false`。
 */
bool terminate_unit_d(void);

/**
 * @brief Unit Dの現在状態を取得する。
 *
 * @param status 現在状態を書き込む出力先。
 * @return 状態を取得できた場合は `true`、出力先が不正な場合は `false`。
 */
bool get_unit_d_status(unit_d_status_t *status);

/**
 * @brief Unit Dの状態値を表示用文字列へ変換する。
 *
 * @param status 文字列へ変換する状態値。
 * @return 状態を表す静的文字列。
 */
const char *convert_unit_d_status_to_string(unit_d_status_t status);

/**
 * @brief Unit Dを指定位置へ移動する。
 *
 * @param position 移動先を表すサンプル値。
 * @return 移動指示を受け付けた場合は `true`、失敗した場合は `false`。
 */
bool move_unit_d(int position);

/**
 * @brief Unit Dを開く。
 *
 * @return open指示を受け付けた場合は `true`、失敗した場合は `false`。
 */
bool open_unit_d(void);

/**
 * @brief Unit Dを閉じる。
 *
 * @return close指示を受け付けた場合は `true`、失敗した場合は `false`。
 */
bool close_unit_d(void);

#endif
