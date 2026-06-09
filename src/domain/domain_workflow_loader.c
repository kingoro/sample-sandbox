/**
 * @file domain_workflow_loader.c
 * @brief JSON Workflow定義を検証済みDomain modelへ変換する。
 */
#include "domain_workflow.h"

#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/** 読込み可能なJSONファイル最大byte数。 */
#define DOMAIN_WORKFLOW_JSON_MAX_BYTES (1024U * 1024U)
/** 対応するWorkflow schema version。 */
#define DOMAIN_WORKFLOW_SCHEMA_VERSION 1U

/** JSON token種別。 */
typedef enum {
    /** 未使用token。 */
    JSON_TOKEN_UNDEFINED = 0,
    /** object token。 */
    JSON_TOKEN_OBJECT,
    /** array token。 */
    JSON_TOKEN_ARRAY,
    /** string token。 */
    JSON_TOKEN_STRING,
    /** number、true、false、nullのtoken。 */
    JSON_TOKEN_PRIMITIVE
} json_token_type_t;

/** JSON source内の1要素を示すtoken。 */
typedef struct {
    /** token種別。 */
    json_token_type_t type;
    /** 内容開始offset。 */
    size_t start;
    /** 内容終了offset。 */
    size_t end;
    /** 直接の子token数。 */
    size_t child_count;
    /** 親token index。rootではSIZE_MAX。 */
    size_t parent;
} json_token_t;

/** Tokenizer実行状態。 */
typedef struct {
    /** 出力token配列。 */
    json_token_t *tokens;
    /** token配列容量。 */
    size_t capacity;
    /** 使用token数。 */
    size_t count;
    /** 現在開いているobjectまたはarray。 */
    size_t current_parent;
} json_parser_t;

/**
 * 新しいtokenを割り当てる。
 *
 * @param parser Tokenizer状態。
 * @param type token種別。
 * @param start 開始offset。
 * @return token index。容量不足時はSIZE_MAX。
 */
static size_t json_allocate_token(
    json_parser_t *parser,
    json_token_type_t type,
    size_t start)
{
    size_t index = SIZE_MAX;

    if (parser->count < parser->capacity) {
        index = parser->count++;
        parser->tokens[index] = (json_token_t){
            .type = type,
            .start = start,
            .end = 0U,
            .child_count = 0U,
            .parent = parser->current_parent,
        };
        if (parser->current_parent != SIZE_MAX) {
            parser->tokens[parser->current_parent].child_count++;
        }
    }
    return index;
}

/**
 * JSON空白文字か判定する。
 *
 * @param value 判定文字。
 * @return 空白ならtrue。
 */
static bool json_is_space(char value)
{
    return value == ' ' || value == '\t'
        || value == '\r' || value == '\n';
}

/**
 * JSON delimiterか判定する。
 *
 * @param value 判定文字。
 * @return delimiterならtrue。
 */
static bool json_is_delimiter(char value)
{
    return json_is_space(value) || value == ','
        || value == ']' || value == '}';
}

/**
 * JSON source内の空白を読み飛ばす。
 *
 * @param source JSON source。
 * @param length source長。
 * @param position 現在位置。更新後は次の非空白位置。
 */
static void json_skip_space(
    const char *source,
    size_t length,
    size_t *position)
{
    while (*position < length && json_is_space(source[*position])) {
        ++(*position);
    }
}

/**
 * 16進文字か判定する。
 *
 * @param value 判定文字。
 * @return 16進文字ならtrue。
 */
static bool json_is_hex(char value)
{
    return (value >= '0' && value <= '9')
        || (value >= 'a' && value <= 'f')
        || (value >= 'A' && value <= 'F');
}

/**
 * JSON string文法を検証して末尾まで進める。
 *
 * @param source JSON source。
 * @param length source長。
 * @param position 開始quote位置。成功時はquote直後。
 * @return 有効なstringならtrue。
 */
static bool json_validate_string(
    const char *source,
    size_t length,
    size_t *position)
{
    bool completed = false;

    if (*position >= length || source[*position] != '"') {
        return false;
    }
    ++(*position);
    while (*position < length && !completed) {
        const unsigned char value =
            (unsigned char)source[*position];

        if (value == '"') {
            ++(*position);
            completed = true;
        } else if (value == '\\') {
            ++(*position);
            if (*position >= length) {
                break;
            }
            const char escape = source[*position];

            if (escape == 'u') {
                for (size_t digit = 0U; digit < 4U; ++digit) {
                    ++(*position);
                    if (*position >= length
                        || !json_is_hex(source[*position])) {
                        return false;
                    }
                }
            } else if (strchr("\"\\/bfnrt", escape) == NULL) {
                return false;
            }
            ++(*position);
        } else if (value < 0x20U) {
            return false;
        } else {
            ++(*position);
        }
    }
    return completed;
}

/**
 * JSON number文法を検証して末尾まで進める。
 *
 * @param source JSON source。
 * @param length source長。
 * @param position number開始位置。成功時はnumber直後。
 * @return 有効なnumberならtrue。
 */
static bool json_consume_integer(
    const char *source,
    size_t length,
    size_t *position)
{
    size_t index = *position;

    if (index < length && source[index] == '-') {
        ++index;
    }
    if (index >= length) {
        return false;
    }
    if (source[index] == '0') {
        ++index;
    } else if (source[index] >= '1' && source[index] <= '9') {
        do {
            ++index;
        } while (index < length
            && source[index] >= '0' && source[index] <= '9');
    } else {
        return false;
    }
    *position = index;
    return true;
}

/**
 * JSON numberの小数部を検証する。
 *
 * @param source JSON source。
 * @param length source長。
 * @param position 現在位置。成功時は小数部直後。
 * @return 小数部がない、または有効な場合true。
 */
static bool json_consume_fraction(
    const char *source,
    size_t length,
    size_t *position)
{
    size_t index = *position;

    if (index >= length || source[index] != '.') {
        return true;
    }
    ++index;
    if (index >= length
        || source[index] < '0' || source[index] > '9') {
        return false;
    }
    while (index < length
        && source[index] >= '0' && source[index] <= '9') {
        ++index;
    }
    *position = index;
    return true;
}

/**
 * JSON numberの指数部を検証する。
 *
 * @param source JSON source。
 * @param length source長。
 * @param position 現在位置。成功時は指数部直後。
 * @return 指数部がない、または有効な場合true。
 */
static bool json_consume_exponent(
    const char *source,
    size_t length,
    size_t *position)
{
    size_t index = *position;

    if (index >= length
        || (source[index] != 'e' && source[index] != 'E')) {
        return true;
    }
    ++index;
    if (index < length
        && (source[index] == '+' || source[index] == '-')) {
        ++index;
    }
    if (index >= length
        || source[index] < '0' || source[index] > '9') {
        return false;
    }
    while (index < length
        && source[index] >= '0' && source[index] <= '9') {
        ++index;
    }
    *position = index;
    return true;
}

/**
 * JSON number文法を検証して末尾まで進める。
 *
 * @param source JSON source。
 * @param length source長。
 * @param position number開始位置。成功時はnumber直後。
 * @return 有効なnumberならtrue。
 */
static bool json_validate_number(
    const char *source,
    size_t length,
    size_t *position)
{
    return json_consume_integer(source, length, position)
        && json_consume_fraction(source, length, position)
        && json_consume_exponent(source, length, position);
}

/**
 * JSON valueを再帰検証する。
 *
 * @param source JSON source。
 * @param length source長。
 * @param position value開始位置。成功時はvalue直後。
 * @param depth 現在の入れ子深度。
 * @return 有効なvalueならtrue。
 */
static bool json_validate_value(
    const char *source,
    size_t length,
    size_t *position,
    size_t depth);

/**
 * JSON array文法を検証する。
 *
 * @param source JSON source。
 * @param length source長。
 * @param position 開始bracket位置。成功時はarray直後。
 * @param depth 現在の入れ子深度。
 * @return 有効なarrayならtrue。
 */
static bool json_validate_array(
    const char *source,
    size_t length,
    size_t *position,
    size_t depth)
{
    bool completed = false;

    ++(*position);
    json_skip_space(source, length, position);
    if (*position < length && source[*position] == ']') {
        ++(*position);
        return true;
    }
    while (*position < length && !completed) {
        if (!json_validate_value(
                source, length, position, depth + 1U)) {
            return false;
        }
        json_skip_space(source, length, position);
        if (*position < length && source[*position] == ',') {
            ++(*position);
            json_skip_space(source, length, position);
        } else if (*position < length && source[*position] == ']') {
            ++(*position);
            completed = true;
        } else {
            return false;
        }
    }
    return completed;
}

/**
 * JSON object文法を検証する。
 *
 * @param source JSON source。
 * @param length source長。
 * @param position 開始brace位置。成功時はobject直後。
 * @param depth 現在の入れ子深度。
 * @return 有効なobjectならtrue。
 */
static bool json_validate_object(
    const char *source,
    size_t length,
    size_t *position,
    size_t depth)
{
    bool completed = false;

    ++(*position);
    json_skip_space(source, length, position);
    if (*position < length && source[*position] == '}') {
        ++(*position);
        return true;
    }
    while (*position < length && !completed) {
        if (!json_validate_string(source, length, position)) {
            return false;
        }
        json_skip_space(source, length, position);
        if (*position >= length || source[*position] != ':') {
            return false;
        }
        ++(*position);
        json_skip_space(source, length, position);
        if (!json_validate_value(
                source, length, position, depth + 1U)) {
            return false;
        }
        json_skip_space(source, length, position);
        if (*position < length && source[*position] == ',') {
            ++(*position);
            json_skip_space(source, length, position);
        } else if (*position < length && source[*position] == '}') {
            ++(*position);
            completed = true;
        } else {
            return false;
        }
    }
    return completed;
}

static bool json_validate_value(
    const char *source,
    size_t length,
    size_t *position,
    size_t depth)
{
    bool valid = false;

    if (depth > 64U || *position >= length) {
        return false;
    }
    if (source[*position] == '{') {
        valid = json_validate_object(
            source, length, position, depth);
    } else if (source[*position] == '[') {
        valid = json_validate_array(
            source, length, position, depth);
    } else if (source[*position] == '"') {
        valid = json_validate_string(source, length, position);
    } else if (source[*position] == '-'
        || (source[*position] >= '0' && source[*position] <= '9')) {
        valid = json_validate_number(source, length, position);
    } else {
        static const char *const literals[] = {
            "true", "false", "null",
        };

        for (size_t index = 0U;
             index < sizeof(literals) / sizeof(literals[0]) && !valid;
             ++index) {
            const size_t literal_length = strlen(literals[index]);

            if (literal_length <= length - *position
                && strncmp(&source[*position],
                    literals[index], literal_length) == 0) {
                *position += literal_length;
                valid = true;
            }
        }
    }
    return valid;
}

/**
 * JSON document全体の文法を検証する。
 *
 * @param source JSON source。
 * @param length source長。
 * @return 末尾まで有効なJSON valueが1つある場合true。
 */
static bool json_validate_document(
    const char *source,
    size_t length)
{
    size_t position = 0U;

    json_skip_space(source, length, &position);
    const bool valid =
        json_validate_value(source, length, &position, 0U);
    json_skip_space(source, length, &position);
    return valid && position == length;
}

/**
 * JSON stringをtokenizeする。
 *
 * @param source JSON source。
 * @param length source長。
 * @param position 開始quote位置。成功時に終了quote位置へ更新する。
 * @param parser Tokenizer状態。
 * @return 成功時true。
 */
static bool json_parse_string(
    const char *source,
    size_t length,
    size_t *position,
    json_parser_t *parser)
{
    const size_t token_index =
        json_allocate_token(parser, JSON_TOKEN_STRING, *position + 1U);
    bool escaped = false;
    bool completed = false;

    if (token_index == SIZE_MAX) {
        return false;
    }
    for (size_t index = *position + 1U; index < length; ++index) {
        const char value = source[index];

        if (escaped) {
            escaped = false;
        } else if (value == '\\') {
            escaped = true;
        } else if (value == '"') {
            parser->tokens[token_index].end = index;
            *position = index;
            completed = true;
            break;
        } else if ((unsigned char)value < 0x20U) {
            break;
        }
    }
    return completed;
}

/**
 * JSON primitiveをtokenizeする。
 *
 * @param source JSON source。
 * @param length source長。
 * @param position primitive開始位置。終了delimiter直前へ更新する。
 * @param parser Tokenizer状態。
 * @return 成功時true。
 */
static bool json_parse_primitive(
    const char *source,
    size_t length,
    size_t *position,
    json_parser_t *parser)
{
    const size_t start = *position;
    size_t end = start;

    while (end < length && !json_is_delimiter(source[end])) {
        if ((unsigned char)source[end] < 0x20U || source[end] == ':') {
            return false;
        }
        ++end;
    }
    if (end == start) {
        return false;
    }
    const size_t token_index =
        json_allocate_token(parser, JSON_TOKEN_PRIMITIVE, start);

    if (token_index == SIZE_MAX) {
        return false;
    }
    parser->tokens[token_index].end = end;
    *position = end - 1U;
    return true;
}

/**
 * objectまたはarrayを閉じる。
 *
 * @param parser Tokenizer状態。
 * @param type 閉じるtoken種別。
 * @param end 終了offset。
 * @return 対応tokenを閉じた場合true。
 */
static bool json_close_container(
    json_parser_t *parser,
    json_token_type_t type,
    size_t end)
{
    const size_t current = parser->current_parent;

    if (current == SIZE_MAX || parser->tokens[current].type != type) {
        return false;
    }
    parser->tokens[current].end = end + 1U;
    parser->current_parent = parser->tokens[current].parent;
    return true;
}

/**
 * JSON sourceの1文字をtokenizerへ反映する。
 *
 * @param source JSON source。
 * @param length source長。
 * @param position 現在位置。stringまたはprimitiveでは末尾へ更新する。
 * @param parser Tokenizer状態。
 * @return 処理成功時true。
 */
static bool json_tokenize_at(
    const char *source,
    size_t length,
    size_t *position,
    json_parser_t *parser)
{
    const char value = source[*position];

    if (value == '{' || value == '[') {
        const json_token_type_t type =
            value == '{' ? JSON_TOKEN_OBJECT : JSON_TOKEN_ARRAY;
        const size_t token_index =
            json_allocate_token(parser, type, *position);

        if (token_index == SIZE_MAX) {
            return false;
        }
        parser->current_parent = token_index;
        return true;
    }
    if (value == '}') {
        return json_close_container(
            parser, JSON_TOKEN_OBJECT, *position);
    }
    if (value == ']') {
        return json_close_container(
            parser, JSON_TOKEN_ARRAY, *position);
    }
    if (value == '"') {
        return json_parse_string(
            source, length, position, parser);
    }
    if (json_is_space(value) || value == ':' || value == ',') {
        return true;
    }
    return json_parse_primitive(
        source, length, position, parser);
}

/**
 * JSON source全体をtokenizeする。
 *
 * @param source JSON source。
 * @param length source長。
 * @param parser 出力先Tokenizer状態。
 * @return 構文上tokenizeできた場合true。
 */
static bool json_tokenize(
    const char *source,
    size_t length,
    json_parser_t *parser)
{
    bool succeeded = true;

    for (size_t index = 0U; index < length && succeeded; ++index) {
        succeeded = json_tokenize_at(
            source, length, &index, parser);
    }
    if (parser->current_parent != SIZE_MAX) {
        succeeded = false;
    }
    return succeeded && parser->count > 0U
        && parser->tokens[0].parent == SIZE_MAX;
}

/**
 * tokenの文字列内容が期待値と一致するか確認する。
 *
 * @param source JSON source。
 * @param token 比較token。
 * @param expected 期待文字列。
 * @return 一致時true。
 */
static bool json_token_equals(
    const char *source,
    const json_token_t *token,
    const char *expected)
{
    const size_t length = token->end - token->start;

    return token->type == JSON_TOKEN_STRING
        && strlen(expected) == length
        && strncmp(&source[token->start], expected, length) == 0;
}

/**
 * objectからkeyに対応する値tokenを探す。
 *
 * @param source JSON source。
 * @param tokens token配列。
 * @param token_count token数。
 * @param object_index object token index。
 * @param key 検索key。
 * @return 値token index。未登録時はSIZE_MAX。
 */
static size_t json_object_find(
    const char *source,
    const json_token_t *tokens,
    size_t token_count,
    size_t object_index,
    const char *key)
{
    size_t result = SIZE_MAX;

    if (object_index >= token_count
        || tokens[object_index].type != JSON_TOKEN_OBJECT) {
        return SIZE_MAX;
    }
    bool duplicate = false;
    for (size_t index = object_index + 1U;
         index + 1U < token_count && !duplicate;
         ++index) {
        if (tokens[index].parent == object_index
            && json_token_equals(source, &tokens[index], key)
            && tokens[index + 1U].parent == object_index) {
            if (result == SIZE_MAX) {
                result = index + 1U;
            } else {
                duplicate = true;
            }
        }
    }
    return duplicate ? SIZE_MAX : result;
}

/**
 * arrayの指定要素tokenを取得する。
 *
 * @param tokens token配列。
 * @param token_count token数。
 * @param array_index array token index。
 * @param element_index 0始まり要素index。
 * @return 要素token index。範囲外時はSIZE_MAX。
 */
static size_t json_array_at(
    const json_token_t *tokens,
    size_t token_count,
    size_t array_index,
    size_t element_index)
{
    size_t found = 0U;
    size_t result = SIZE_MAX;

    if (array_index >= token_count
        || tokens[array_index].type != JSON_TOKEN_ARRAY) {
        return SIZE_MAX;
    }
    for (size_t index = array_index + 1U;
         index < token_count && result == SIZE_MAX;
         ++index) {
        if (tokens[index].parent == array_index) {
            if (found == element_index) {
                result = index;
            }
            ++found;
        }
    }
    return result;
}

/**
 * primitive tokenをuint64_tへ変換する。
 *
 * @param source JSON source。
 * @param token 変換token。
 * @param out_value 変換値格納先。
 * @return 10進非負整数へ変換できた場合true。
 */
static bool json_read_u64(
    const char *source,
    const json_token_t *token,
    uint64_t *out_value)
{
    const size_t length = token->end - token->start;
    char buffer[32];
    char *end = NULL;
    unsigned long long value;

    if (token->type != JSON_TOKEN_PRIMITIVE
        || length == 0U || length >= sizeof(buffer)) {
        return false;
    }
    (void)memcpy(buffer, &source[token->start], length);
    buffer[length] = '\0';
    errno = 0;
    value = strtoull(buffer, &end, 10);
    if (errno != 0 || end == buffer || *end != '\0'
        || buffer[0] == '-') {
        return false;
    }
    *out_value = (uint64_t)value;
    return true;
}

/**
 * string tokenを新しいNUL終端文字列へcopyする。
 *
 * @param source JSON source。
 * @param token copy元token。
 * @return 呼出側所有文字列。失敗時はNULL。
 */
static char *json_copy_string(
    const char *source,
    const json_token_t *token)
{
    const size_t length = token->end - token->start;
    char *copy = NULL;

    if (token->type == JSON_TOKEN_STRING && length > 0U
        && memchr(&source[token->start], '\\', length) == NULL) {
        copy = malloc(length + 1U);
        if (copy != NULL) {
            (void)memcpy(copy, &source[token->start], length);
            copy[length] = '\0';
        }
    }
    return copy;
}

/**
 * objectの必須uint32_t fieldを読む。
 *
 * @param source JSON source。
 * @param tokens token配列。
 * @param token_count token数。
 * @param object_index object index。
 * @param key field名。
 * @param out_value 値格納先。
 * @return fieldが存在しuint32_tへ変換できた場合true。
 */
static bool json_object_u32(
    const char *source,
    const json_token_t *tokens,
    size_t token_count,
    size_t object_index,
    const char *key,
    uint32_t *out_value)
{
    const size_t value_index = json_object_find(
        source, tokens, token_count, object_index, key);
    uint64_t value = 0U;

    return value_index != SIZE_MAX
        && json_read_u64(source, &tokens[value_index], &value)
        && value <= UINT32_MAX
        && ((*out_value = (uint32_t)value), true);
}

/**
 * objectの必須uint64_t fieldを読む。
 *
 * @param source JSON source。
 * @param tokens token配列。
 * @param token_count token数。
 * @param object_index object index。
 * @param key field名。
 * @param out_value 値格納先。
 * @return fieldが存在しuint64_tへ変換できた場合true。
 */
static bool json_object_u64(
    const char *source,
    const json_token_t *tokens,
    size_t token_count,
    size_t object_index,
    const char *key,
    uint64_t *out_value)
{
    const size_t value_index = json_object_find(
        source, tokens, token_count, object_index, key);

    return value_index != SIZE_MAX
        && json_read_u64(source, &tokens[value_index], out_value);
}

/**
 * objectの必須string fieldをcopyする。
 *
 * @param source JSON source。
 * @param tokens token配列。
 * @param token_count token数。
 * @param object_index object index。
 * @param key field名。
 * @return 呼出側所有文字列。失敗時はNULL。
 */
static char *json_object_string(
    const char *source,
    const json_token_t *tokens,
    size_t token_count,
    size_t object_index,
    const char *key)
{
    const size_t value_index = json_object_find(
        source, tokens, token_count, object_index, key);

    return value_index == SIZE_MAX
        ? NULL : json_copy_string(source, &tokens[value_index]);
}

/**
 * Scenario内部の動的領域を解放する。
 *
 * @param scenario 解放対象。構造体本体は解放しない。
 */
static void destroy_scenario(domain_scenario_t *scenario)
{
    domain_sequence_t *sequences =
        (domain_sequence_t *)scenario->sequences;

    if (sequences != NULL) {
        for (size_t sequence_index = 0U;
             sequence_index < scenario->sequence_count;
             ++sequence_index) {
            free((void *)sequences[sequence_index].name);
            free((void *)sequences[sequence_index].steps);
        }
    }
    free((void *)scenario->name);
    free(sequences);
    *scenario = (domain_scenario_t){0};
}

/* Catalog内部の確保領域を解放する。公開契約はheaderへ記載する。 */
void domain_workflow_catalog_destroy(domain_workflow_catalog_t *catalog)
{
    if (catalog != NULL) {
        for (size_t entry_index = 0U;
             entry_index < catalog->entry_count;
             ++entry_index) {
            destroy_scenario(&catalog->entries[entry_index].scenario);
        }
        free(catalog->entries);
        free(catalog);
    }
}

/**
 * 1 Step定義をparseする。
 *
 * @param source JSON source。
 * @param tokens token配列。
 * @param token_count token数。
 * @param object_index Step object index。
 * @param unit_count 利用可能Unit数。
 * @param out_step 変換先。
 * @return schemaが有効な場合true。
 */
static bool parse_step(
    const char *source,
    const json_token_t *tokens,
    size_t token_count,
    size_t object_index,
    size_t unit_count,
    domain_step_t *out_step)
{
    const size_t command_index = json_object_find(
        source, tokens, token_count, object_index, "command");
    uint32_t unit_number = 0U;
    uint64_t timeout_ms = 0U;

    if (tokens[object_index].type != JSON_TOKEN_OBJECT
        || !json_object_u32(source, tokens, token_count,
            object_index, "unit", &unit_number)
        || unit_number == 0U || unit_number > unit_count
        || command_index == SIZE_MAX
        || !json_token_equals(
            source, &tokens[command_index], "execute")
        || !json_object_u64(source, tokens, token_count,
            object_index, "timeout_ms", &timeout_ms)
        || timeout_ms == 0U) {
        return false;
    }
    *out_step = (domain_step_t){
        .unit_index = (size_t)unit_number - 1U,
        .command = UNIT_MOCK_COMMAND_EXECUTE,
        .timeout_ms = timeout_ms,
    };
    return true;
}

/**
 * 1 Sequence定義をparseする。
 *
 * @param source JSON source。
 * @param tokens token配列。
 * @param token_count token数。
 * @param object_index Sequence object index。
 * @param unit_count 利用可能Unit数。
 * @param out_sequence 変換先。
 * @return 読込み結果。
 */
static domain_workflow_load_result_t parse_sequence(
    const char *source,
    const json_token_t *tokens,
    size_t token_count,
    size_t object_index,
    size_t unit_count,
    domain_sequence_t *out_sequence)
{
    const size_t steps_index = json_object_find(
        source, tokens, token_count, object_index, "steps");
    char *name = json_object_string(
        source, tokens, token_count, object_index, "name");
    domain_workflow_load_result_t result =
        DOMAIN_WORKFLOW_LOAD_SCHEMA_ERROR;

    if (tokens[object_index].type != JSON_TOKEN_OBJECT
        || name == NULL || steps_index == SIZE_MAX
        || tokens[steps_index].type != JSON_TOKEN_ARRAY
        || tokens[steps_index].child_count == 0U) {
        free(name);
        return result;
    }
    domain_step_t *steps = calloc(
        tokens[steps_index].child_count, sizeof(*steps));

    if (steps == NULL) {
        free(name);
        return DOMAIN_WORKFLOW_LOAD_NO_MEMORY;
    }
    result = DOMAIN_WORKFLOW_LOAD_OK;
    for (size_t index = 0U;
         index < tokens[steps_index].child_count
            && result == DOMAIN_WORKFLOW_LOAD_OK;
         ++index) {
        const size_t step_index = json_array_at(
            tokens, token_count, steps_index, index);

        if (step_index == SIZE_MAX
            || !parse_step(source, tokens, token_count,
                step_index, unit_count, &steps[index])) {
            result = DOMAIN_WORKFLOW_LOAD_SCHEMA_ERROR;
        }
    }
    if (result == DOMAIN_WORKFLOW_LOAD_OK) {
        *out_sequence = (domain_sequence_t){
            .name = name,
            .steps = steps,
            .step_count = tokens[steps_index].child_count,
        };
    } else {
        free(name);
        free(steps);
    }
    return result;
}

/**
 * ScenarioのSequence配列をparseする。
 *
 * @param source JSON source。
 * @param tokens token配列。
 * @param token_count token数。
 * @param array_index Sequence array index。
 * @param unit_count 利用可能Unit数。
 * @param out_sequences 生成配列格納先。
 * @param out_total_steps 合計Step数格納先。
 * @return 読込み結果。
 */
static domain_workflow_load_result_t parse_sequences(
    const char *source,
    const json_token_t *tokens,
    size_t token_count,
    size_t array_index,
    size_t unit_count,
    domain_sequence_t **out_sequences,
    size_t *out_total_steps)
{
    const size_t sequence_count = tokens[array_index].child_count;
    domain_sequence_t *sequences =
        calloc(sequence_count, sizeof(*sequences));

    if (sequences == NULL) {
        return DOMAIN_WORKFLOW_LOAD_NO_MEMORY;
    }
    domain_workflow_load_result_t result = DOMAIN_WORKFLOW_LOAD_OK;
    size_t parsed_count = 0U;
    size_t total_steps = 0U;

    for (size_t index = 0U;
         index < sequence_count && result == DOMAIN_WORKFLOW_LOAD_OK;
         ++index) {
        const size_t sequence_index = json_array_at(
            tokens, token_count, array_index, index);

        result = sequence_index == SIZE_MAX
            ? DOMAIN_WORKFLOW_LOAD_SCHEMA_ERROR
            : parse_sequence(source, tokens, token_count,
                sequence_index, unit_count, &sequences[index]);
        if (result == DOMAIN_WORKFLOW_LOAD_OK) {
            parsed_count++;
            if (sequences[index].step_count > SIZE_MAX - total_steps) {
                result = DOMAIN_WORKFLOW_LOAD_SCHEMA_ERROR;
            } else {
                total_steps += sequences[index].step_count;
            }
        }
    }
    if (result == DOMAIN_WORKFLOW_LOAD_OK) {
        *out_sequences = sequences;
        *out_total_steps = total_steps;
    } else {
        for (size_t index = 0U; index < parsed_count; ++index) {
            free((void *)sequences[index].name);
            free((void *)sequences[index].steps);
        }
        free(sequences);
    }
    return result;
}

/**
 * 1 Scenario定義をparseする。
 *
 * @param source JSON source。
 * @param tokens token配列。
 * @param token_count token数。
 * @param object_index Scenario object index。
 * @param unit_count 利用可能Unit数。
 * @param out_scenario 変換先。
 * @return 読込み結果。
 */
static domain_workflow_load_result_t parse_scenario(
    const char *source,
    const json_token_t *tokens,
    size_t token_count,
    size_t object_index,
    size_t unit_count,
    domain_scenario_t *out_scenario)
{
    const size_t sequences_index = json_object_find(
        source, tokens, token_count, object_index, "sequences");
    char *name = json_object_string(
        source, tokens, token_count, object_index, "name");
    uint32_t scenario_id = 0U;
    domain_workflow_load_result_t result =
        DOMAIN_WORKFLOW_LOAD_SCHEMA_ERROR;

    if (tokens[object_index].type != JSON_TOKEN_OBJECT
        || !json_object_u32(source, tokens, token_count,
            object_index, "id", &scenario_id)
        || scenario_id == 0U || name == NULL
        || sequences_index == SIZE_MAX
        || tokens[sequences_index].type != JSON_TOKEN_ARRAY
        || tokens[sequences_index].child_count == 0U) {
        free(name);
        return result;
    }
    domain_sequence_t *sequences = NULL;
    size_t total_steps = 0U;
    result = parse_sequences(
        source, tokens, token_count, sequences_index,
        unit_count, &sequences, &total_steps);
    if (result == DOMAIN_WORKFLOW_LOAD_OK) {
        *out_scenario = (domain_scenario_t){
            .id = scenario_id,
            .name = name,
            .sequences = sequences,
            .sequence_count = tokens[sequences_index].child_count,
            .total_step_count = total_steps,
        };
    } else {
        free(name);
    }
    return result;
}

/**
 * Workflow entryの検索keyが既存entryと重複するか確認する。
 *
 * @param entries entry配列。
 * @param count 検査済みentry数。
 * @param feature 機能ID。
 * @param condition 選択条件。
 * @param scenario_id Scenario ID。
 * @return keyまたはScenario IDが重複する場合true。
 */
static bool entry_is_duplicate(
    const domain_workflow_entry_t *entries,
    size_t count,
    uint32_t feature,
    uint32_t condition,
    uint32_t scenario_id)
{
    bool duplicate = false;

    for (size_t index = 0U; index < count && !duplicate; ++index) {
        duplicate =
            (entries[index].feature == feature
                && entries[index].condition == condition)
            || entries[index].scenario.id == scenario_id;
    }
    return duplicate;
}

/**
 * 1 Workflow entryをparseする。
 *
 * @param source JSON source。
 * @param tokens token配列。
 * @param token_count token数。
 * @param object_index Workflow object index。
 * @param unit_count 利用可能Unit数。
 * @param entries 検査済みentry配列。
 * @param parsed_count 検査済みentry数。
 * @param out_entry 変換先。
 * @return 読込み結果。
 */
static domain_workflow_load_result_t parse_entry(
    const char *source,
    const json_token_t *tokens,
    size_t token_count,
    size_t object_index,
    size_t unit_count,
    const domain_workflow_entry_t *entries,
    size_t parsed_count,
    domain_workflow_entry_t *out_entry)
{
    const size_t scenario_index = json_object_find(
        source, tokens, token_count, object_index, "scenario");
    uint32_t feature = 0U;
    uint32_t condition = 0U;

    if (tokens[object_index].type != JSON_TOKEN_OBJECT
        || !json_object_u32(source, tokens, token_count,
            object_index, "feature", &feature)
        || feature == 0U
        || !json_object_u32(source, tokens, token_count,
            object_index, "condition", &condition)
        || scenario_index == SIZE_MAX) {
        return DOMAIN_WORKFLOW_LOAD_SCHEMA_ERROR;
    }
    domain_scenario_t scenario = {0};
    domain_workflow_load_result_t result = parse_scenario(
        source, tokens, token_count, scenario_index, unit_count, &scenario);

    if (result == DOMAIN_WORKFLOW_LOAD_OK
        && entry_is_duplicate(entries, parsed_count,
            feature, condition, scenario.id)) {
        destroy_scenario(&scenario);
        result = DOMAIN_WORKFLOW_LOAD_SCHEMA_ERROR;
    }
    if (result == DOMAIN_WORKFLOW_LOAD_OK) {
        *out_entry = (domain_workflow_entry_t){
            .feature = feature,
            .condition = condition,
            .scenario = scenario,
        };
    }
    return result;
}

/**
 * Token列からWorkflow Catalogを構築する。
 *
 * @param source JSON source。
 * @param tokens token配列。
 * @param token_count token数。
 * @param unit_count 利用可能Unit数。
 * @param out_catalog 生成Catalog格納先。
 * @return 読込み結果。
 */
static domain_workflow_load_result_t build_catalog(
    const char *source,
    const json_token_t *tokens,
    size_t token_count,
    size_t unit_count,
    domain_workflow_catalog_t **out_catalog)
{
    const size_t version_index = json_object_find(
        source, tokens, token_count, 0U, "schema_version");
    const size_t workflows_index = json_object_find(
        source, tokens, token_count, 0U, "workflows");
    uint64_t version = 0U;

    if (tokens[0].type != JSON_TOKEN_OBJECT
        || version_index == SIZE_MAX
        || !json_read_u64(source, &tokens[version_index], &version)
        || version != DOMAIN_WORKFLOW_SCHEMA_VERSION
        || workflows_index == SIZE_MAX
        || tokens[workflows_index].type != JSON_TOKEN_ARRAY
        || tokens[workflows_index].child_count == 0U) {
        return DOMAIN_WORKFLOW_LOAD_SCHEMA_ERROR;
    }
    domain_workflow_catalog_t *catalog = calloc(1U, sizeof(*catalog));

    if (catalog == NULL) {
        return DOMAIN_WORKFLOW_LOAD_NO_MEMORY;
    }
    catalog->entries = calloc(
        tokens[workflows_index].child_count, sizeof(*catalog->entries));
    if (catalog->entries == NULL) {
        free(catalog);
        return DOMAIN_WORKFLOW_LOAD_NO_MEMORY;
    }
    domain_workflow_load_result_t result = DOMAIN_WORKFLOW_LOAD_OK;
    for (size_t index = 0U;
         index < tokens[workflows_index].child_count
            && result == DOMAIN_WORKFLOW_LOAD_OK;
         ++index) {
        const size_t entry_index = json_array_at(
            tokens, token_count, workflows_index, index);

        result = entry_index == SIZE_MAX
            ? DOMAIN_WORKFLOW_LOAD_SCHEMA_ERROR
            : parse_entry(source, tokens, token_count, entry_index,
                unit_count, catalog->entries, catalog->entry_count,
                &catalog->entries[index]);
        if (result == DOMAIN_WORKFLOW_LOAD_OK) {
            catalog->entry_count++;
        }
    }
    if (result == DOMAIN_WORKFLOW_LOAD_OK) {
        *out_catalog = catalog;
    } else {
        domain_workflow_catalog_destroy(catalog);
    }
    return result;
}

/**
 * JSONファイル全体をheapへ読み込む。
 *
 * @param path 読込みpath。
 * @param out_source source格納先。
 * @param out_length source長格納先。
 * @return 読込み結果。
 */
static domain_workflow_load_result_t read_json_file(
    const char *path,
    char **out_source,
    size_t *out_length)
{
    FILE *file = fopen(path, "rb");

    if (file == NULL) {
        return DOMAIN_WORKFLOW_LOAD_IO_ERROR;
    }
    domain_workflow_load_result_t result = DOMAIN_WORKFLOW_LOAD_IO_ERROR;
    if (fseek(file, 0L, SEEK_END) == 0) {
        const long file_size = ftell(file);

        if (file_size > 0
            && (unsigned long)file_size <= DOMAIN_WORKFLOW_JSON_MAX_BYTES
            && fseek(file, 0L, SEEK_SET) == 0) {
            char *source = malloc((size_t)file_size + 1U);

            if (source == NULL) {
                result = DOMAIN_WORKFLOW_LOAD_NO_MEMORY;
            } else if (fread(source, 1U, (size_t)file_size, file)
                != (size_t)file_size) {
                free(source);
            } else {
                source[file_size] = '\0';
                *out_source = source;
                *out_length = (size_t)file_size;
                result = DOMAIN_WORKFLOW_LOAD_OK;
            }
        }
    }
    (void)fclose(file);
    return result;
}

domain_workflow_load_result_t domain_workflow_catalog_load_json_file(
    const char *path,
    size_t unit_count,
    domain_workflow_catalog_t **out_catalog)
{
    char *source = NULL;
    size_t source_length = 0U;
    domain_workflow_load_result_t result;

    if (path == NULL || path[0] == '\0'
        || unit_count == 0U || out_catalog == NULL) {
        return DOMAIN_WORKFLOW_LOAD_INVALID_ARGUMENT;
    }
    *out_catalog = NULL;
    result = read_json_file(path, &source, &source_length);
    if (result != DOMAIN_WORKFLOW_LOAD_OK) {
        return result;
    }
    const size_t token_capacity = (source_length / 2U) + 2U;
    json_token_t *tokens = calloc(token_capacity, sizeof(*tokens));

    if (tokens == NULL) {
        free(source);
        return DOMAIN_WORKFLOW_LOAD_NO_MEMORY;
    }
    json_parser_t parser = {
        .tokens = tokens,
        .capacity = token_capacity,
        .count = 0U,
        .current_parent = SIZE_MAX,
    };
    if (!json_validate_document(source, source_length)
        || !json_tokenize(source, source_length, &parser)) {
        result = DOMAIN_WORKFLOW_LOAD_PARSE_ERROR;
    } else {
        result = build_catalog(
            source, tokens, parser.count, unit_count, out_catalog);
    }
    free(tokens);
    free(source);
    return result;
}

const domain_scenario_t *domain_workflow_catalog_find(
    const domain_workflow_catalog_t *catalog,
    uint32_t feature,
    uint32_t condition)
{
    const domain_scenario_t *scenario = NULL;

    if (catalog != NULL) {
        for (size_t index = 0U;
             index < catalog->entry_count && scenario == NULL;
             ++index) {
            if (catalog->entries[index].feature == feature
                && catalog->entries[index].condition == condition) {
                scenario = &catalog->entries[index].scenario;
            }
        }
    }
    return scenario;
}
