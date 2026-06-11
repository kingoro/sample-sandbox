/**
 * @file domain_event_publisher.c
 * @brief Event Foundation PublisherへDomain Eventを接続するadapter実装。
 */
#include "domain_event_publisher.h"

#include <pthread.h>
#include <stdlib.h>

/** Publisherが保持できる未配送Domain Event数。 */
#define DOMAIN_EVENT_QUEUE_CAPACITY 32U
/** Publisherへ登録できるDomain購読数。 */
#define DOMAIN_EVENT_SUBSCRIPTION_CAPACITY 16U

/**
 * Domain Event Publisher内部状態。
 */
struct domain_event_publisher {
    /** payload値copyと同期配送を行うEvent Foundation Publisher。 */
    ut_event_publisher_t publisher;
    /** Event Queue storage。 */
    ut_event_t event_storage[DOMAIN_EVENT_QUEUE_CAPACITY];
    /** alignmentを保証したDomain payload storage。 */
    domain_event_payload_t payload_storage[DOMAIN_EVENT_QUEUE_CAPACITY];
    /** payload slot使用状態storage。 */
    uint8_t occupied_storage[DOMAIN_EVENT_QUEUE_CAPACITY];
    /** Dispatcher購読storage。 */
    ut_event_subscription_t
        subscription_storage[DOMAIN_EVENT_SUBSCRIPTION_CAPACITY];
    /** Unit workerとControl thread間を保護するmutex。 */
    pthread_mutex_t mutex;
};

/**
 * Event Foundation Publisher共有領域へ入る。
 *
 * @param context pthread_mutex_tへのポインタ。
 */
static void domain_event_lock(void *context)
{
    (void)pthread_mutex_lock(context);
}

/**
 * Event Foundation Publisher共有領域から出る。
 *
 * @param context pthread_mutex_tへのポインタ。
 */
static void domain_event_unlock(void *context)
{
    (void)pthread_mutex_unlock(context);
}

domain_event_publisher_t *domain_event_publisher_create(void)
{
    domain_event_publisher_t *publisher =
        calloc(1U, sizeof(*publisher));

    if (publisher == NULL) {
        return NULL;
    }
    if (pthread_mutex_init(&publisher->mutex, NULL) != 0) {
        free(publisher);
        return NULL;
    }
    if (ut_event_publisher_init(
            &publisher->publisher,
            publisher->event_storage,
            (uint8_t *)publisher->payload_storage,
            publisher->occupied_storage,
            DOMAIN_EVENT_QUEUE_CAPACITY,
            sizeof(publisher->payload_storage[0]),
            publisher->subscription_storage,
            DOMAIN_EVENT_SUBSCRIPTION_CAPACITY,
            domain_event_lock,
            domain_event_unlock,
            &publisher->mutex) != UT_EVENT_OK) {
        (void)pthread_mutex_destroy(&publisher->mutex);
        free(publisher);
        publisher = NULL;
    }
    return publisher;
}

void domain_event_publisher_destroy(domain_event_publisher_t *publisher)
{
    if (publisher != NULL) {
        (void)pthread_mutex_destroy(&publisher->mutex);
        free(publisher);
    }
}

ut_event_result_t domain_event_publisher_subscribe(
    domain_event_publisher_t *publisher,
    uint32_t event_id,
    ut_event_handler_t handler,
    void *context)
{
    if (publisher == NULL) {
        return UT_EVENT_INVALID_ARGUMENT;
    }
    return ut_event_publisher_subscribe(
        &publisher->publisher,
        event_id,
        handler,
        context);
}

ut_event_result_t domain_event_publisher_publish(
    domain_event_publisher_t *publisher,
    uint32_t event_id,
    uint32_t source,
    const domain_event_payload_t *payload)
{
    if (publisher == NULL) {
        return UT_EVENT_INVALID_ARGUMENT;
    }
    return ut_event_publisher_publish_copy(
        &publisher->publisher,
        event_id,
        source,
        payload,
        sizeof(*payload));
}

void domain_event_publisher_on_unit_result(
    const unit_mock_result_t *result,
    void *context)
{
    domain_event_publisher_t *publisher = context;

    if (publisher != NULL && result != NULL) {
        const domain_event_payload_t payload = {
            .unit_request_id = result->request_id,
            .unit_number = result->unit_number,
            .error = result->error,
        };
        const uint32_t event_id =
            result->error == UNIT_MOCK_ERROR_NONE
            ? DOMAIN_EVENT_UNIT_COMPLETED
            : DOMAIN_EVENT_UNIT_ERROR;

        (void)domain_event_publisher_publish(
            publisher,
            event_id,
            result->unit_number,
            &payload);
    }
}

ut_event_result_t domain_event_publisher_dispatch(
    domain_event_publisher_t *publisher,
    size_t budget,
    size_t *out_dispatched)
{
    if (publisher == NULL) {
        return UT_EVENT_INVALID_ARGUMENT;
    }
    return ut_event_publisher_dispatch(
        &publisher->publisher,
        budget,
        out_dispatched);
}
