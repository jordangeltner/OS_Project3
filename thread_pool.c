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
	p->queue = NULL;
	p->standbylist = NULL;
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
	
	pool_task_t* task = (pool_task_t*)malloc(sizeof(pool_task_t));
	setup_connection(&connfd,pool,task);
	if(task==NULL){
		return err;
	}
	if(task->priority<1 || task->priority >3){
		task->priority = 3;
	}
	pthread_mutex_lock(&pool->queue_lock);
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
	while(sem_wait(pool->threadsem,&pool->threadsem_lock)==-1){usleep(100);}
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
 	err+= pthread_mutex_destroy(&pool->sblock);
 	err+= pthread_mutex_destroy(&pool->cancellock);
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
	
	//get the lock on the queue
	if(pthread_mutex_lock(&p->queue_lock)==EDEADLK){printf("DEADLOCKED QUEUE_LOCK\n"); fflush(stdout);}
    while(p->active ==1) {
		
		//if I'm the thread that just cancelled a seat
		if(p->sbtid==pthread_self()){
			pthread_mutex_unlock(&p->queue_lock);
			
			//look for matching task in standbylist
			pthread_mutex_lock(&p->sblock);
			pool_task_t* task = p->standbylist;
			pool_task_t* prev = NULL;
			while(task!=NULL){
				//if we find the matching seat in the standby list remove it
				if(task->seat_id==p->last_cancelled){
					if(prev!=NULL){
						prev->next = task->next;
					}
					else{
						p->standbylist = task->next;
					}
					pthread_mutex_unlock(&p->sblock);
					sem_post(p->sbsem,&p->sbsem_lock);
					
					//update the last cancelled so we don't try to run it again
					p->last_cancelled = -1;
					p->sbtid = 0;
					pthread_mutex_unlock(&p->cancellock);
					
					//run the connection
					load_connection(task,p);
					break;
				}
				prev = task;
				task = task->next;
			}
			//if we got to the end of the list without finding a match, update
			if(task==NULL){
				pthread_mutex_unlock(&p->sblock);
				p->last_cancelled = -1;
				p->sbtid = 0;
				pthread_mutex_unlock(&p->cancellock);
			}
			pthread_mutex_lock(&p->queue_lock);
		}
		sem_post(p->threadsem,&p->threadsem_lock);
		
		//wait for a new task to be added
		pthread_cond_wait(&p->notify,&p->queue_lock);
		pool_task_t* curr = p->queue;
		pool_task_t* best = curr;
		pool_task_t* prev = NULL;
		pool_task_t* bestprev = NULL;
		
		//search through the list for the best matching task, i.e. highest priority (lowest number)
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
		
		//run the connection and go back into the waiting loop
		load_connection(best,p);
		pthread_mutex_lock(&p->queue_lock);
    }
    pthread_exit(NULL);
    return(NULL);
}
