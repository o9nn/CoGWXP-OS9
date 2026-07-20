/**
 * @file timer.c
 * @brief Periodic timer implementation.
 */

#include "cogutil.h"

#include <pthread.h>
#include <stdlib.h>

struct cog_timer {
    uint32_t interval_ms;
    cog_timer_callback_t callback;
    void* user_data;
    pthread_t thread;
    pthread_mutex_t lock;
    bool running;
    bool thread_active;
};

static void* timer_thread(void* arg) {
    cog_timer_t timer = (cog_timer_t)arg;

    while (true) {
        cog_time_sleep_ms(timer->interval_ms);

        pthread_mutex_lock(&timer->lock);
        bool running = timer->running;
        cog_timer_callback_t callback = timer->callback;
        void* user_data = timer->user_data;
        pthread_mutex_unlock(&timer->lock);

        if (!running) break;
        if (callback) callback(user_data);
    }

    return NULL;
}

COGUTIL_API cog_timer_t cog_timer_create(uint32_t interval_ms,
                                         cog_timer_callback_t callback,
                                         void* user_data) {
    if (interval_ms == 0) interval_ms = 1;

    cog_timer_t timer = (cog_timer_t)calloc(1, sizeof(struct cog_timer));
    if (!timer) return NULL;

    timer->interval_ms = interval_ms;
    timer->callback = callback;
    timer->user_data = user_data;
    if (pthread_mutex_init(&timer->lock, NULL) != 0) {
        free(timer);
        return NULL;
    }

    return timer;
}

COGUTIL_API void cog_timer_destroy(cog_timer_t timer) {
    if (!timer) return;
    cog_timer_stop(timer);
    pthread_mutex_destroy(&timer->lock);
    free(timer);
}

COGUTIL_API void cog_timer_start(cog_timer_t timer) {
    if (!timer) return;

    pthread_mutex_lock(&timer->lock);
    if (timer->running) {
        pthread_mutex_unlock(&timer->lock);
        return;
    }

    timer->running = true;
    timer->thread_active = true;
    pthread_mutex_unlock(&timer->lock);

    if (pthread_create(&timer->thread, NULL, timer_thread, timer) != 0) {
        pthread_mutex_lock(&timer->lock);
        timer->running = false;
        timer->thread_active = false;
        pthread_mutex_unlock(&timer->lock);
    }
}

COGUTIL_API void cog_timer_stop(cog_timer_t timer) {
    if (!timer) return;

    pthread_mutex_lock(&timer->lock);
    bool active = timer->thread_active;
    timer->running = false;
    timer->thread_active = false;
    pthread_mutex_unlock(&timer->lock);

    if (active) pthread_join(timer->thread, NULL);
}

COGUTIL_API void cog_timer_reset(cog_timer_t timer) {
    if (!timer) return;

    bool restart = false;
    pthread_mutex_lock(&timer->lock);
    restart = timer->running;
    pthread_mutex_unlock(&timer->lock);

    if (restart) {
        cog_timer_stop(timer);
        cog_timer_start(timer);
    }
}
