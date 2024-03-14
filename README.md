# Introduction
`cpool` is a simple C thread pool library implemented with C11 `<threads.h>`.

It uses a ring buffer as a bounded job queue, whose size is specified on pool creation.

## Future
`cpool` has a useful little feature somewhat akin to `std::future` in C++.
When enqueuing a job, you can optionally receive a _future_ handle associated with the job.
This provides an easy way to wait on individual jobs, without the need for manual synchronization.

# Example usage
```c
#include "cpool.h"
#include <stdio.h>
#include <threads.h>
#include <stdlib.h>

static void sleep(void* arg)
{
	(void)arg;
	thrd_sleep(&(struct timespec) { .tv_sec = 1 }, NULL);
}

static void print_int(void* arg)
{
	thrd_sleep(&(struct timespec) { .tv_sec = 1 }, NULL);
	printf("%d\n", *(int*)arg);
	free(arg);
}

int main(void)
{
	cpool* tp = cpool_create(8, 8); // creates pool with 8 worker threads, job queue capacity 8.
	if (!tp) return 1;

	cpool_future* fut;
	cpool_enqueue(tp, sleep, NULL, &fut);

	puts("Waiting for future...");
	cpool_wait_future(fut);
	puts("Waiting for pool...");
	cpool_wait(tp);
	puts("pool finished");

	for (int i = 0; i < 20; ++i) {
		int* arg = malloc(sizeof(arg));
		if (!arg) break;
		*arg = i;
		cpool_enqueue(tp, print_int, arg, NULL); // enqueue w/o using future. This will not incur extra overhead of future.
	}
	
	cpool_wait(tp);
	puts("pool finished");

	cpool_destroy(tp);
	puts("pool destroyed");
}
```

**Possible output:**
```
Waiting for future...
Waiting for pool...
pool finished
7
0
2
6
1
3
5
4
8
14
9
15
13
11
10
12
19
16
17
18
pool finished
pool destroyed
```
