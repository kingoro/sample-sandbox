/**
 * @file unit_mock.h
 * @brief 入出力ポートを持つ非同期Mock Unit。
 *
 * 各UnitはInputで命令を受け、専用ワーカーで処理し、Outputへ状態と結果を公開する。
 */
#ifndef UNIT_MOCK_H
#define UNIT_MOCK_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Mock Unitへ与える命令。
 */
typedef enum {
    /** Unitへ何も要求しない。 */
    UNIT_MOCK_COMMAND_NONE = 0,
    /** Unit固有の操作を開始する。 */
    UNIT_MOCK_COMMAND_EXECUTE
} unit_mock_command_t;

/**
 * Mock Unitの実行状態。
 */
typedef enum {
    /** 命令を待っている状態。 */
    UNIT_MOCK_STATUS_IDLE = 0,
    /** 命令を実行している状態。 */
    UNIT_MOCK_STATUS_RUNNING,
    /** 命令が正常完了した状態。 */
    UNIT_MOCK_STATUS_COMPLETED,
    /** 命令がエラー終了した状態。 */
    UNIT_MOCK_STATUS_ERROR
} unit_mock_status_t;

/**
 * Mock Unitのエラー。
 */
typedef enum {
    /** エラーなし。 */
    UNIT_MOCK_ERROR_NONE = 0,
    /** Mock処理中に待機処理が失敗した。 */
    UNIT_MOCK_ERROR_EXECUTION
} unit_mock_error_t;

/**
 * Input書き込み結果。
 */
typedef enum {
    /** Inputを受け付けた。 */
    UNIT_MOCK_INPUT_ACCEPTED = 0,
    /** Unitが実行中のためInputを受け付けなかった。 */
    UNIT_MOCK_INPUT_BUSY,
    /** InputまたはUnitが無効だった。 */
    UNIT_MOCK_INPUT_INVALID_ARGUMENT
} unit_mock_input_result_t;

/**
 * 上位層からMock Unitへ渡すInput。
 */
typedef struct {
    /** Unitへ要求する命令。 */
    unit_mock_command_t command;
    /** 要求と完了を対応付ける上位層発行の識別子。 */
    uint32_t request_id;
} unit_mock_input_t;

/**
 * Mock Unitから上位層へ返すOutput。
 */
typedef struct {
    /** Unitの現在状態。 */
    unit_mock_status_t status;
    /** 現在処理中、または最後に完了した要求の識別子。 */
    uint32_t request_id;
    /** 現在、または最後の要求で発生したエラー。 */
    unit_mock_error_t error;
} unit_mock_output_t;

/**
 * Unitの非同期処理結果。
 */
typedef struct {
    /** 結果を通知した1始まりのUnit番号。 */
    uint8_t unit_number;
    /** 完了または失敗した要求ID。 */
    uint32_t request_id;
    /** 処理結果。 */
    unit_mock_error_t error;
} unit_mock_result_t;

/**
 * Unitの非同期処理結果を受け取るcallback。
 *
 * callbackはUnit worker thread上で呼ばれる。resultの値はcallback中のみ有効であり、
 * 後で使用する場合は値を複製する。
 *
 * @param result 完了またはエラー結果。
 * @param context 利用側が登録したcontext。
 */
typedef void (*unit_mock_result_handler_t)(const unit_mock_result_t *result, void *context);

/**
 * Unitの非同期結果通知先。
 */
typedef struct {
    /** 処理完了時に呼ぶcallback。 */
    unit_mock_result_handler_t handler;
    /** handlerへ渡すcontext。 */
    void *context;
} unit_mock_result_output_t;

/**
 * Mock Unitの内部状態を隠蔽する型。
 */
typedef struct unit_mock unit_mock_t;

/**
 * 指定番号のMock Unitを生成する。
 *
 * @param unit_number 1から10までのUnit番号。
 * @return 生成したUnit。引数不正または生成失敗時はNULL。
 *
 * 戻り値の所有権は呼び出し元へ移る。使用後はunit_mock_destroy()へ渡す。
 */
unit_mock_t *unit_mock_create(uint8_t unit_number);

/**
 * Unitの非同期結果通知先を設定する。
 *
 * @param unit 設定するUnit。
 * @param result_output callbackとcontext。値を複製する。
 * @return 設定成功時UNIT_MOCK_INPUT_ACCEPTED、引数不正時
 * UNIT_MOCK_INPUT_INVALID_ARGUMENT、実行中はUNIT_MOCK_INPUT_BUSY。
 *
 * contextはUnit破棄後まで有効に保つ。
 */
unit_mock_input_result_t unit_mock_set_result_output(unit_mock_t *unit, const unit_mock_result_output_t *result_output);

/**
 * Mock Unitを破棄する。
 *
 * @param unit unit_mock_create()が返したUnit。NULLも許容する。
 *
 * 実行中の場合は処理完了を待つ。呼び出し後、unitが指す領域の寿命は終了する。
 * 同じUnitへの別threadからの操作と同時に呼び出してはならない。
 */
void unit_mock_destroy(unit_mock_t *unit);

/**
 * Mock UnitのInputへ命令を書き込む。
 *
 * @param unit Inputを書き込むUnit。
 * @param input 書き込むInput。関数内で値を複製し、ポインタを保持しない。
 * @return Inputの受付結果。
 *
 * UNIT_MOCK_COMMAND_EXECUTEを受け付けると非同期処理を開始する。
 * この関数はthread-safeであり、処理完了を待たずに戻る。
 */
unit_mock_input_result_t unit_mock_write_input(unit_mock_t *unit, const unit_mock_input_t *input);

/**
 * Mock UnitのOutputを読み取る。
 *
 * @param unit Outputを読み取るUnit。
 * @param output 読み取った値の格納先。
 * @return 読み取り成功時はUNIT_MOCK_INPUT_ACCEPTED、引数不正時は
 * UNIT_MOCK_INPUT_INVALID_ARGUMENT。
 *
 * 値は呼び出し時点のsnapshotであり、呼び出し元が所有するoutputへ複製する。
 * この関数はthread-safeであり、処理完了を待たずに戻る。
 */
unit_mock_input_result_t unit_mock_read_output(unit_mock_t *unit, unit_mock_output_t *output);

#ifdef __cplusplus
}
#endif

#endif
