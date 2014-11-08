#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>

#include "thread_pool.h"
#include "util.h"

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


static void *thread_do_work(void *pool);


/*
 * Create a threadpool, initialize variables, etc
 *
 */
pool_t *pool_create(int queue_size, int num_threads)
{
	pool_t* p = malloc(sizeof(pool_t));
	p->active = 1;
	pthread_mutex_t q;
	int i;
	pthread_t threads[num_threads];
	pthread_mutex_init(&q,NULL);
	p->queue_lock = q;
	for(i=0;i<20;i++){
		pthread_mutex_t s;
		pthread_mutex_init(&s,NULL);
		p->seat_locks[i] = s; 
	}
	pthread_cond_t notify;
	pthread_cond_init(&notify,NULL);
	p->notify = notify;
	p->queue = NULL;
	p->standbylist = NULL;
	p->trystandbylist = 0;
	pthread_mutex_t sblock;
	pthread_mutex_init(&sblock,NULL);
	pthread_cond_t sbnotify;
	pthread_cond_init(&sbnotify,NULL);
	p->sbnotify = sbnotify;
	p->sblock = sblock;
	m_sem_t* sbsem = malloc(sizeof(m_sem_t));
	sbsem->value = STANDBY_SIZE;
	p->sbsem = sbsem;
	p->thread_count = num_threads;
	p->task_queue_size_limit = queue_size;
	for (i=0;i<num_threads;i++){
		pthread_create(&threads[i],NULL,thread_do_work,p);
	}
	p->threads = threads;
    return p;
}


/*
 * Add a task to the threadpool
 *
 */
int pool_add_task(pool_t *pool, int connfd)
{
    int err = 0;
    //LOCK THE QUEUE
	// single threaded
	//handle_connection(&connfd);
	pthread_mutex_lock(&pool->queue_lock);
	// multi threaded
	//add connection to pool queue
	pool_task_t* task = (pool_task_t*)malloc(sizeof(pool_task_t));
	task->connfd = connfd;
	task->function = NULL;
	task->argument = NULL;
	task->next = (void*)pool->queue;
	pool->queue = task;
	//UNLOCK THE QUEUE
	pthread_mutex_unlock(&pool->queue_lock);
	//Signal the waiting thread
	pthread_cond_signal(&pool->notify);
    return err;
}



/*
 * Destroy the threadpool, free all memory, destroy treads, etc
 *
 */
int pool_destroy(pool_t *pool)
{
    int err = 0;
 	err+=pthread_mutex_destroy(&pool->queue_lock);
 	int i;
 	for(i=0;i<20;i++){
 		err+=pthread_mutex_destroy(&pool->seat_locks[i]);
 	}
 	err+= pthread_cond_destroy(&pool->notify);
 	err+= pthread_cond_destroy(&pool->sbnotify);
 	err+= pthread_mutex_destroy(&pool->sblock);
 	free(pool->sbsem);
 	pool_task_t* q = pool->queue;
 	while(q!=NULL){
 		pool_task_t* curr = q;
 		q = q->next;
 		free(curr);
 	}
 	q = pool->standbylist;
 	while(q!=NULL){
 		pool_task_t* curr = q;
 		q = q->next;
 		free(curr);
 	}
 	pool->active = 0; //forces every thread to pthread_exit from thread_do_work
    return err;
}



/*
 * Work loop for threads. Should be passed into the pthread_create() method.
 *
 */
static void *thread_do_work(void *pool)
{ 
	pool_t* p = (pool_t*)pool;
    while(p->active ==1) {
    	pthread_cond_wait(&p->notify,&p->queue_lock); //Release and acquire lock
    //	printf("About to handle connection\n"); fflush(stdout);
    	handle_connection(&p->queue->connfd,p);
    	printf("Handled connection\n"); fflush(stdout);
    }
    pthread_exit(NULL);
    return(NULL);
}
