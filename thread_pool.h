#ifndef _THREADPOOL_H_
#define _THREADPOOL_H_


#define NUM_SEATS 20 //CHECK WITH PIAZZA


typedef struct pool_task_t {
    void (*function)(void *);
    void *argument;
    int connfd;
    void *next; //Used to link up queue
} pool_task_t;

typedef struct pool_t {
  pthread_mutex_t queue_lock; //protects the worker queue
  pthread_mutex_t seat_locks[NUM_SEATS];
  pthread_cond_t notify;
  pthread_t *threads;
  pool_task_t *queue;
  pool_task_t *standbylist; //The standby list
  int trystandbylist; //0 or 1 that indicates whether or not we need to try tasks 
  						// from the standby list
  int thread_count;
  int task_queue_size_limit;
  int active;
} pool_t;



pool_t *pool_create(int thread_count, int queue_size);

int pool_add_task(pool_t *pool, int connfd);

int pool_destroy(pool_t *pool);

#endif
