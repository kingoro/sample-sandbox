#ifndef UNIT_E_H
#define UNIT_E_H

#include <stdbool.h>

/**
 * @file unit_e.h
 * @brief Unit Eの公開インターフェース。
 *
 * Unit Eは、`domain` 層のStepから呼び出される部品操作モジュールのサンプルです。
 * 関数名は「動詞_対象」の順番にして、何をする関数かを先頭で読み取れるようにしています。
 * 現時点では実機操作は行わず、各APIが呼ばれたことを標準出力へ表示します。
 */

/**
 * @brief Unit Eの状態。
 */
typedef enum {
    /** 初期化されていない状態。 */
    UNIT_E_STATUS_NOT_INITIALIZED = 0,
    /** 初期化済みで操作を受け付けられる状態。 */
    UNIT_E_STATUS_READY,
    /** open操作後の状態。 */
    UNIT_E_STATUS_OPEN,
    /** move操作後の状態。 */
    UNIT_E_STATUS_MOVING,
    /** close操作後の状態。 */
    UNIT_E_STATUS_CLOSED
} unit_e_status_t;

/**
 * @brief Unit Eを初期化する。
 *
 * @return 初期化できた場合は `true`、失敗した場合は `false`。
 */
bool initialize_unit_e(void);

/**
 * @brief Unit Eを終了する。
 *
 * @return 終了できた場合は `true`、失敗した場合は `false`。
 */
bool terminate_unit_e(void);

/**
 * @brief Unit Eの現在状態を取得する。
 *
 * @param status 現在状態を書き込む出力先。
 * @return 状態を取得できた場合は `true`、出力先が不正な場合は `false`。
 */
bool get_unit_e_status(unit_e_status_t *status);

/**
 * @brief Unit Eの状態値を表示用文字列へ変換する。
 *
 * @param status 文字列へ変換する状態値。
 * @return 状態を表す静的文字列。
 */
const char *convert_unit_e_status_to_string(unit_e_status_t status);

/**
 * @brief Unit Eを指定位置へ移動する。
 *
 * @param position 移動先を表すサンプル値。
 * @return 移動指示を受け付けた場合は `true`、失敗した場合は `false`。
 */
bool move_unit_e(int position);

/**
 * @brief Unit Eを開く。
 *
 * @return open指示を受け付けた場合は `true`、失敗した場合は `false`。
 */
bool open_unit_e(void);

/**
 * @brief Unit Eを閉じる。
 *
 * @return close指示を受け付けた場合は `true`、失敗した場合は `false`。
 */
bool close_unit_e(void);

#endif
