#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#include "thread_pool.h"

/**
 *  @struct threadpool_task
 *  @brief the work struct
 *
 *  Feel free to make any modifications you want to the function prototypes and structs
 *
 *  @var function Pointer to the function that will perform the task.
 *  @var argument Argument to be passed to the function.
 */

#define MAX_THREADS 20
#define STANDBY_SIZE 8
#define NUM_SEATS 20 //CHECK WITH PIAZZA

typedef struct {
    void (*function)(void *);
    void *argument;
    int connfd;
    void *next; //Used to link up queue
} pool_task_t;


struct pool_t {
  pthread_mutex_t queue_lock; //protects the worker queue
  pthread_mutex_t seat_locks[NUM_SEATS];
  pthread_cond_t notify;
  pthread_t *threads;
  pool_task_t *queue;
  //pool_task_t *standbylist; //The standby list
  //bool trystandbylist; //bool that indicates whether or not we need to try tasks 
  						// from the standby list
  int thread_count;
  int task_queue_size_limit;
};

static void *thread_do_work(void *pool);


/*
 * Create a threadpool, initialize variables, etc
 *
 */
pool_t *pool_create(int queue_size, int num_threads)
{
	
    return NULL;
}


/*
 * Add a task to the threadpool
 *
 */
int pool_add_task(pool_t *pool, void (*function)(void *), void *argument)
{
    int err = 0;
        
    return err;
}



/*
 * Destroy the threadpool, free all memory, destroy treads, etc
 *
 */
int pool_destroy(pool_t *pool)
{
    int err = 0;
 
    return err;
}



/*
 * Work loop for threads. Should be passed into the pthread_create() method.
 *
 */
static void *thread_do_work(void *pool)
{ 
	pool_t* p = (pool_t*)pool;
    while(1) {
    	pthread_cond_wait(&p->notify,&p->queue_lock); //Release and acquire lock
    	handle_connection(&p->queue->connfd);
    }

    pthread_exit(NULL);
    return(NULL);
}
