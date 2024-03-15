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

/* opaque future object */
typedef struct cpool_future cpool_future;

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
 * @brief  Add a job to the pool. Optionally outputs a future handle.
 * 
 * Blocks until job queue has available slot.
 *
 * @param[in]  func Job function to run
 * @param[in]  data Argument for `func`
 * @param[out] future pointer to a handle(pointer) to future, used to retrieve the resultant future handle.
 *                    No future will be created and output if `future` is NULL.
 *                    Otherwise, after successful enqueue, `*future` will be a pointer to the future object
 *                    associated with this job. Or if the enqueue was unsuccessful, or future creation failed,
 *                    `*future` will be NULL.
 * @return 0 on success, 1 if pool is stopped.
 *
 * @note If `data` owns any resources, either `func` is responsible for cleaning up,
 *       or the user does this outside of the pool, depending on the scope of `data`.
 * 
 * @attention If a non-NULL future handle was output, it *MUST* be consumed by `cpool_wait_future()`
 *            in order to release its resources.
 *            If you do not wish to use the future output, just pass `future` as NULL.
*/
int cpool_enqueue(cpool* pool, cpool_func_t func, void* data, cpool_future** future);

/**
 * @brief Wait until all jobs are finished, i.e. no worker is doing work and job queue is empty.
 *
 * @note  This does not consider enqueuing jobs.
*/
void cpool_wait(cpool* pool);

/**
 * @brief Wait on the future handle, returning when the associated job has finished.
 * 
 * @attention Only one thread can wait on a future, and only once.
*/
void cpool_wait_future(cpool_future* future);

/**
 * Request stop.
 *
 * After requesting stop, enqueuing is rejected.
 * Workers will finish remaining jobs in the queue, then exit.
 * This function call only signals to stop. It does not wait for workers to finish.
*/
void cpool_stop(cpool* pool);

#ifdef __cplusplus
}
#endif

#endif /* CPOOL_H */
