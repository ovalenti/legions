#include <time.h>
#include <stdlib.h>
#include <string.h>

#include <lwip/sys.h>
#include <lwip/timeouts.h>
#include <sloop/loop.h>

u32_t sys_now(void) {
    struct timespec t;

    clock_gettime(CLOCK_MONOTONIC, &t);
    return (u32_t)(t.tv_sec * 1000 + t.tv_nsec / 1000000);
}

struct glue_cyclic_timer {
    struct loop_timeout loop_timeout;
    int lwip_timer_idx;
};

static void glue_cyclic_timer_cb(struct loop_timeout *loop_timeout) {
    struct glue_cyclic_timer *t = container_of(loop_timeout, struct glue_cyclic_timer, loop_timeout);
    const struct lwip_cyclic_timer *lwip_timer;

    lwip_timer = &lwip_cyclic_timers[t->lwip_timer_idx];

    lwip_timer->handler();

    loop_timeout_add(loop_timeout, lwip_timer->interval_ms);
}

static struct glue_cyclic_timer *glue_cyclic_timers;

void sys_timeouts_init(void) {
    int i;
    glue_cyclic_timers = (struct glue_cyclic_timer *)malloc(lwip_num_cyclic_timers * sizeof(struct glue_cyclic_timer));
    memset(glue_cyclic_timers, 0, lwip_num_cyclic_timers * sizeof(struct glue_cyclic_timer));

    for (i = 0; i < lwip_num_cyclic_timers; i++) {
        struct glue_cyclic_timer *t = glue_cyclic_timers + i;

        t->loop_timeout.cb = &glue_cyclic_timer_cb;
        t->lwip_timer_idx = i;

        loop_timeout_add(&(t->loop_timeout), lwip_cyclic_timers[t->lwip_timer_idx].interval_ms);
    }
}
