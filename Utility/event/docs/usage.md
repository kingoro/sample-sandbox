# 利用方法

## 初期化

```c
#include "utility_event.h"

static ut_event_t event_storage[16];
static ut_event_queue_t event_queue;

static ut_event_subscription_t subscription_storage[8];
static ut_event_dispatcher_t dispatcher;

void app_event_init(void)
{
    (void)ut_event_queue_init(
        &event_queue, event_storage, 16u);
    (void)ut_event_dispatcher_init(
        &dispatcher, subscription_storage, 8u);
}
```

## 発行と処理

```c
enum {
    APP_EVENT_PRINT_DATA_READY = 1u
};

static void on_print_data_ready(
    const ut_event_t *event,
    void *user_context)
{
    (void)user_context;
    /* event->payloadをこの呼出し中に利用する。 */
}

void app_register_handlers(void)
{
    (void)ut_event_subscribe(
        &dispatcher,
        APP_EVENT_PRINT_DATA_READY,
        on_print_data_ready,
        NULL);
}

ut_event_result_t app_post_print_data(const void *data, size_t size)
{
    const ut_event_t event = {
        APP_EVENT_PRINT_DATA_READY,
        2u,
        data,
        size
    };

    return ut_event_queue_push(&event_queue, &event);
}

void app_event_loop_step(void)
{
    ut_event_t event;

    if (ut_event_queue_pop(&event_queue, &event) == UT_EVENT_OK) {
        (void)ut_event_dispatch(&dispatcher, &event, NULL);
    }
}
```

Queueはpayload本体をcopyしない。この例で`data`が一時変数や再利用される受信領域を
指す場合は不正になる。非同期に保持するデータはBuffer Poolへcopyし、その参照を
Eventへ設定し、handler完了後に所有権規則に従って解放する。

`app_post_print_data`が`UT_EVENT_FULL`を返した場合、呼出側は再試行、明示的な破棄、
fault遷移などapplicationで定めた方針を実行する。
