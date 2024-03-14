#ifndef CPOOL_H
#define CPOOL_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* job function type */
typedef void (*cpool_func_t)(void*);

/* opaque pool struct */
typedef struct cpool cpool;

/**
 * @brief Allocate and initialize a thread pool.
 *
 * @param[in] nb_workers Number of worker threads. Must be positive.
 * @param[in] max_jobs   Capacity of the job queue. Must be positive.
 * @return A pointer to an initialized pool, or NULL on failure.
*/
cpool* cpool_create(size_t nb_workers, size_t max_jobs);

/**
 * @brief Request stop, wait for workers to exit, clean-up resources, and finally return.
*/
void cpool_destroy(cpool* pool);

/**
 * @brief  Add a job to the pool. Blocks until job queue has available slot.
 *
 * @param[in] func Job function to run
 * @param[in] data Argument for `func`
 * @return 0 on success, 1 if pool is stopped.
 *
 * @note If `data` owns any resources, either `func` is responsible for cleaning up,
 *       or the user does this outside of the pool, depending on the scope of `data`.
*/
int cpool_enqueue(cpool* pool, cpool_func_t func, void* data);

/**
 * @brief Wait until all jobs are finished, i.e. no worker is doing work and job queue is empty.
 *
 * @note  This does not consider enqueuing jobs.
*/
void cpool_wait(cpool* pool);

/**
 * Request stop. Does not wait.
 *
 * After requesting stop, enqueuing is rejected.
 * Workers will finish remaining jobs in the queue, then exit.
*/
void cpool_stop(cpool* pool);

#ifdef __cplusplus
}
#endif

#endif /* CPOOL_H */
