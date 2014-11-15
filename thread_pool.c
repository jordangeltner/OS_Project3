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
	pthread_mutexattr_t errattr;
	pthread_mutexattr_init(&errattr);
	pthread_mutexattr_settype(&errattr,PTHREAD_MUTEX_ERRORCHECK);
	p->active = 1;
	p->last_cancelled = -1;
	pthread_mutex_t q;
	pthread_mutex_t c;
	pthread_mutex_init(&c,&errattr);
	p->cancellock = c;
	pthread_mutex_t d;
	pthread_mutex_init(&d,&errattr);
	p->try_sblock = d;
	int i;
	pthread_t threads[num_threads];
	pthread_mutex_init(&q,&errattr);
	p->queue_lock = q;
	for(i=0;i<20;i++){
		pthread_mutex_t s;
		pthread_mutex_init(&s,&errattr);
		p->seat_locks[i] = s; 
	}
	pthread_cond_t notify;
	pthread_cond_init(&notify,NULL);
	p->notify = notify;
	pthread_cond_t task_notify;
	pthread_cond_init(&task_notify,NULL);
	p->notify = task_notify;
	p->queue = NULL;
	p->standbylist = NULL;
	p->trystandbylist = 0;
	pthread_mutex_t sblock;
	pthread_mutex_init(&sblock,&errattr);
	pthread_mutex_t sbsem_lock;
	pthread_mutex_init(&sbsem_lock,&errattr);
  	pthread_mutex_t seatsem_lock;
  	pthread_mutex_init(&seatsem_lock,&errattr);
  	pthread_mutex_t threadsem_lock;
  	pthread_mutex_init(&threadsem_lock,&errattr);
	p->sbsem_lock = sbsem_lock;
	p->seatsem_lock = seatsem_lock;
	p->threadsem_lock = threadsem_lock;
	pthread_cond_t sbnotify;
	pthread_cond_init(&sbnotify,NULL);
	p->sbnotify = sbnotify;
	p->sblock = sblock;
	m_sem_t* sbsem = malloc(sizeof(m_sem_t));
	sbsem->value = 8;
	p->sbsem = sbsem;
	m_sem_t* seatsem = malloc(sizeof(m_sem_t));
	seatsem->value = 20;
	p->seatsem = seatsem;
	m_sem_t* threadsem = malloc(sizeof(m_sem_t));
	threadsem->value = 0;
	p->threadsem = threadsem;
	p->thread_count = num_threads;
	p->task_queue_size_limit = queue_size;
	for (i=0;i<num_threads;i++){
		pthread_create(&threads[i],NULL,thread_do_work,p);
	}
	p->threads = threads;
	pthread_t sbtid = 0;
	p->sbtid = sbtid;
	
    return p;
}


/*
 * Add a task to the threadpool
 *
 */
int pool_add_task(pool_t *pool, int connfd)
{
    int err = 0;
	// multi threaded
	//add connection to pool queue
	pool_task_t* task = (pool_task_t*)malloc(sizeof(pool_task_t));
	setup_connection(&connfd,pool,task);
	if(task==NULL){
		return err;
	}
	if(task->priority<1 || task->priority >3){
		task->priority = 3;
	}
	pthread_mutex_lock(&pool->queue_lock);
 	printf("ACQUIRED QUEUE LOCK %d\n",pthread_self()); LINE; fflush(stdout);
	pool_task_t* curr = pool->queue;
	pool_task_t* prev = NULL;
	while(curr!=NULL){
		prev = curr;
		curr = curr->next;
	}
	if(prev==NULL){
		pool->queue = task;
	}
	else{
		prev->next = task;
	}
	
	//UNLOCK THE QUEUE
	pthread_mutex_unlock(&pool->queue_lock);
	//Signal the waiting thread
	printf("RELEASED QUEUE LOCK %d\n",pthread_self()); fflush(stdout);
	while(sem_wait(pool->threadsem,&pool->threadsem_lock)==-1){LINE; usleep(100);}
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
 	err+= pthread_cond_destroy(&pool->task_notify);
 	err+= pthread_mutex_destroy(&pool->sblock);
 	err+= pthread_mutex_destroy(&pool->cancellock);
 	err+= pthread_mutex_destroy(&pool->try_sblock);
 	err+= pthread_mutex_destroy(&pool->seatsem_lock);
 	err+= pthread_mutex_destroy(&pool->sbsem_lock);
 	err+= pthread_mutex_destroy(&pool->threadsem_lock);
 	free(pool->sbsem);
 	free(pool->seatsem);
 	free(pool->threadsem);
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
	if(pthread_mutex_lock(&p->queue_lock)==EDEADLK){LINE; printf("DEADLOCKED QUEUE_LOCK %d\n",pthread_self()); fflush(stdout);}
    while(p->active ==1) {
		
		if(p->sbtid==pthread_self()){
			pthread_mutex_unlock(&p->queue_lock);
			printf("RELEASED QUEUE LOCK %d\n",pthread_self()); LINE; fflush(stdout);
			//look for matching task in standbylist
			LINE;
			pthread_mutex_lock(&p->sblock);
			printf("ACQUIRED STANDBY LOCK %d\n",pthread_self()); LINE; fflush(stdout);
			pool_task_t* task = p->standbylist;
			pool_task_t* prev = NULL;
			while(task!=NULL){
				if(task->seat_id==p->last_cancelled){
					if(prev!=NULL){
						prev->next = task->next;
					}
					else{
						p->standbylist = task->next;
					}
					pthread_mutex_unlock(&p->sblock);
					printf("RELEASED STANDBY LOCK %d\n",pthread_self()); LINE; fflush(stdout);
					sem_post(p->sbsem,&p->sbsem_lock);
					p->last_cancelled = -1;
					p->sbtid = 0;
					pthread_mutex_unlock(&p->cancellock);
					load_connection(task,p);
					break;
				}
				prev = task;
				task = task->next;
			}
			if(task==NULL){
				pthread_mutex_unlock(&p->sblock);
				printf("RELEASED STANDBY LOCK %d\n",pthread_self()); LINE; fflush(stdout);
				p->last_cancelled = -1;
				p->sbtid = 0;
				pthread_mutex_unlock(&p->cancellock);
			}
			LINE;
			pthread_mutex_lock(&p->queue_lock);
			printf("ACQUIRED QUEUE LOCK %d\n",pthread_self()); LINE; fflush(stdout);
		}
		printf("RELEASING QUEUE LOCK %d\n",pthread_self()); fflush(stdout);
		sem_post(p->threadsem,&p->threadsem_lock);
		pthread_cond_wait(&p->notify,&p->queue_lock);
		//sem_wait(p->sbsem,&p->sbsem_lock);
		//if(pthread_cond_wait(&p->notify,&p->queue_lock)==EINVAL){LINE; printf("QUEUE_LOCK NOT OWNED WHEN PTHREAD_COND_WAIT IS CALLED\n"); fflush(stdout);} //Release and acquire lock
		printf("ACQUIRED QUEUE LOCK %d\n",pthread_self()); fflush(stdout);
		pool_task_t* curr = p->queue;
		pool_task_t* best = curr;
		pool_task_t* prev = NULL;
		pool_task_t* bestprev = NULL;
		while(curr!=NULL){
			if(curr->priority < best->priority){
				best = curr;
				bestprev = prev;
			}
			prev = curr;
			curr = curr->next;
		}
		if(bestprev==NULL){
			p->queue = p->queue->next;
		}
		else{
			bestprev->next = best->next;
		}
		pthread_mutex_unlock(&p->queue_lock);
		printf("RELEASED QUEUE LOCK %d\n", pthread_self()); LINE; fflush(stdout);
		//handle_connection(cfd,p);
		load_connection(best,p);
		pthread_mutex_lock(&p->queue_lock);
		printf("ACQUIRED QUEUE LOCK %d\n",pthread_self()); LINE; fflush(stdout);
		printf("Handled connection\n"); fflush(stdout);
    }
    pthread_exit(NULL);
    return(NULL);
}
