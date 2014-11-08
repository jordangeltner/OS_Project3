#ifndef _THREADPOOL_H_
#define _THREADPOOL_H_

#include "util.h"




pool_t *pool_create(int thread_count, int queue_size);

int pool_add_task(pool_t *pool, int connfd);

int pool_destroy(pool_t *pool);

#endif
