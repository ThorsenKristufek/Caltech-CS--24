#include "thread_pool.h"

#include <assert.h>
#include <pthread.h>
#include <stdlib.h>
typedef struct thread_pool {
    pthread_t *p_list;
    queue_t *queue;
    size_t elements_count;
} thread_pool_t;

typedef struct work {
    work_function_t func;
    void *aux;
} work_t;

void *thread_help_func(void *queue_ptr) {
    queue_t *queue = (queue_t *) queue_ptr;
    while (true) {
        void *element = queue_dequeue(queue);
        if (element == NULL) {
            return NULL;
        }
        work_t *work = (work_t *) element;
        void *auxload = work->aux;
        (work->func)(auxload);
        free(element);
    }
    return NULL;
}

thread_pool_t *thread_pool_init(size_t num_worker_threads) {
    pthread_t *ptr = (pthread_t *) calloc(num_worker_threads, sizeof(pthread_t));
    assert(ptr != NULL);

    thread_pool_t *thread_pool = (thread_pool_t *) malloc(sizeof(thread_pool_t));
    assert(thread_pool != NULL);

    thread_pool->queue = queue_init();

    thread_pool->p_list = ptr;
    thread_pool->elements_count = num_worker_threads;

    for (size_t i = 0; i < num_worker_threads; i++) {
        pthread_create(&ptr[i], NULL, thread_help_func, thread_pool->queue);
    }
    return thread_pool;
}

void thread_pool_add_work(thread_pool_t *pool, work_function_t function, void *aux) {
    work_t *work = (work_t *) malloc(sizeof(work_t));
    if (work == NULL) {
        return;
    }
    work->func = function;
    work->aux = aux;
    queue_t *queue = pool->queue;
    queue_enqueue(queue, (void *) work);
}

void thread_pool_finish(thread_pool_t *pool) {
    for (size_t i = 0; i < pool->elements_count; i++) {
        queue_enqueue(pool->queue, NULL);
    }
    for (size_t j = 0; j < pool->elements_count; j++) {
        pthread_join(pool->p_list[j], NULL);
    }
    queue_free(pool->queue);
    free(pool->p_list);
    free(pool);
}