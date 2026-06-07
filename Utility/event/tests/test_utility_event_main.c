#include "test_support.h"

int run_utility_event_queue_tests(void);
int run_utility_event_dispatcher_tests(void);

int main(void)
{
    CHECK(run_utility_event_queue_tests() == 0);
    CHECK(run_utility_event_dispatcher_tests() == 0);
    return 0;
}
