#include "cpool.h"
#include <threads.h>
#include <assert.h>
#include <stdlib.h>

typedef struct {
    cpool_func_t func;
    void* data;
    // cpool_work_clean_func clean_func;
} cpool_work;

struct cpool_s {
    thrd_t* workers;     /* Allocated array of thread identifiers. Joined on destruction. */
    size_t nb_workers;

    cpool_work* jobs;    /* ring buffer of jobs */
    size_t max_jobs;     /* max size of the ring buffer */
    size_t job_first, job_count;

    mtx_t mutex;
    cnd_t cond, cond_enqueue, cond_idle;
    size_t nb_working;
    int stop;
};

static int
thread_func(void* pool_ptr) {
    cpool* pool = pool_ptr;
    for (;;) {
        cpool_func_t job_func;
        void* job_data;
        {
            mtx_lock(&pool->mutex);
            while (pool->job_count == 0 && !pool->stop) {
                cnd_wait(&pool->cond, &pool->mutex);
            }
            if (pool->stop && pool->job_count == 0) {
                mtx_unlock(&pool->mutex);
                return 0;
            }
            /* get a job from front */
            cpool_work* job_front = pool->jobs + pool->job_first;
            job_func = job_front->func;
            job_data = job_front->data;
            pool->job_first = (pool->job_first + 1) % pool->max_jobs;
            pool->job_count -= 1;
            pool->nb_working += 1;
            mtx_unlock(&pool->mutex);
        }

        cnd_signal(&pool->cond_enqueue);

        job_func(job_data);

        {
            mtx_lock(&pool->mutex);
            if (--pool->nb_working == 0) cnd_signal(&pool->cond_idle);
            mtx_unlock(&pool->mutex);
        }
    }
}

cpool*
cpool_create(size_t nb_workers, size_t max_jobs)
{
    cpool* pool = NULL;
    if (!nb_workers || !max_jobs) goto end;

    pool = malloc(sizeof(cpool));
    if (!pool) goto end;
    pool->nb_workers = nb_workers;
    pool->max_jobs   = max_jobs;
    pool->job_first  = 0;
    pool->job_count  = 0;
    pool->nb_working = 0;
    pool->stop       = 0;

    if (!(pool->workers = malloc(sizeof(thrd_t) * nb_workers))) goto workers_fail;
    if (!(pool->jobs = malloc(sizeof(cpool_work) * max_jobs))) goto jobs_fail;
    if (mtx_init(&pool->mutex, mtx_plain) != thrd_success) goto mutex_fail;
    if (cnd_init(&pool->cond) != thrd_success) goto cond_fail;
    if (cnd_init(&pool->cond_enqueue) != thrd_success) goto cond_enqueue_fail;
    if (cnd_init(&pool->cond_idle) != thrd_success) goto cond_idle_fail;

    /* launch workers */
    size_t thread_success_count = 0;
    for (size_t i = 0; i < nb_workers; ++i) {
        if (thrd_create(pool->workers + i, thread_func, pool) == thrd_success) {
            ++thread_success_count;
        }
        else break;
    }
    //FIXME: clean-up threads in case of failure!
    assert(thread_success_count == nb_workers);
    goto end;

cond_idle_fail:
    cnd_destroy(&pool->cond_enqueue);
cond_enqueue_fail:
    cnd_destroy(&pool->cond);
cond_fail:
    mtx_destroy(&pool->mutex);
mutex_fail:
    free(pool->jobs);
jobs_fail:
    free(pool->workers);
workers_fail:
    free(pool);
    pool = NULL;
end:
    return pool;
}

void
cpool_destroy(cpool* pool)
{
    cpool_stop(pool);
    for (size_t i = 0; i < pool->nb_workers; ++i) {
        thrd_join(pool->workers[i], NULL);
    }
    cnd_destroy(&pool->cond_idle);
    cnd_destroy(&pool->cond_enqueue);
    cnd_destroy(&pool->cond);
    mtx_destroy(&pool->mutex);
    free(pool->jobs);
    free(pool->workers);
    free(pool);
}

int
cpool_enqueue(cpool* pool, cpool_func_t func, void* data)
{
    {
        mtx_lock(&pool->mutex);
        while (pool->job_count == pool->max_jobs && !pool->stop) {
            cnd_wait(&pool->cond_enqueue, &pool->mutex);
        }
        if (pool->stop) {
            mtx_unlock(&pool->mutex);
            return 1;
        }
        /* push back work */
        cpool_work* job_new = pool->jobs + (pool->job_first + pool->job_count) % pool->max_jobs;
        job_new->func = func;
        job_new->data = data;
        pool->job_count += 1;
        mtx_unlock(&pool->mutex);
    }
    cnd_signal(&pool->cond);
    return 0;
}

void
cpool_stop(cpool* pool)
{
    {
        mtx_lock(&pool->mutex);
        pool->stop = 1;
        mtx_unlock(&pool->mutex);
    }
    cnd_broadcast(&pool->cond);
    cnd_broadcast(&pool->cond_enqueue);
}

void
cpool_wait(cpool* pool)
{
    mtx_lock(&pool->mutex);
    while (pool->nb_working > 0 || pool->job_count > 0) {
        cnd_wait(&pool->cond_idle, &pool->mutex);
    }
    mtx_unlock(&pool->mutex);
}
