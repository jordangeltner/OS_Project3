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
static int nooneyet = 0;
// static int tflag = 1;
// static int xflag = 0;

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
	p->sbsem_lock = sbsem_lock;
	p->seatsem_lock = seatsem_lock;
	pthread_cond_t sbnotify;
	pthread_cond_init(&sbnotify,NULL);
	p->sbnotify = sbnotify;
	p->sblock = sblock;
	m_sem_t* sbsem = malloc(sizeof(m_sem_t));
	sbsem->value = 0;
	p->sbsem = sbsem;
	m_sem_t* seatsem = malloc(sizeof(m_sem_t));
	seatsem->value = 20;
	p->seatsem = seatsem;
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
//     tflag =1;
//     nooneyet = 1;
    //LOCK THE QUEUE
	// single threaded
	//handle_connection(&connfd);
	LINE; fflush(stdout);
 	//if(nooneyet<=0){nooneyet++; usleep(59);} else{nooneyet--; usleep(77);}
//	if(pthread_mutex_lock(&pool->queue_lock)==EDEADLK){LINE; printf("DEADLOCKED QUEUE_LOCK\n"); fflush(stdout); exit(EXIT_FAILURE);}
// 	}
// 	else{
// 		pthread_cond_wait(&pool->task_notify,&pool->queue_lock);
// 	}
// 	while(xflag==1 && nooneyet == 1){usleep(3);}
// 	if(nooneyet>=1){
// 		printf("RELEASING QUEUE LOCK\n"); LINE; fflush(stdout);
// 		pthread_cond_wait(&pool->task_notify,&pool->queue_lock);
// 		//while(pthread_mutex_trylock(&pool->queue_lock)!=0){ sleep(10);}
// 		printf("ACQUIRED QUEUE LOCK\n"); fflush(stdout);
// 	}
// 	else{ nooneyet++;}
	LINE; fflush(stdout);
	// multi threaded
	//add connection to pool queue
	pool_task_t* task = (pool_task_t*)malloc(sizeof(pool_task_t));
	task->connfd = connfd;
	task->seat_id = -1;
	task->next = NULL;
// 	printf("connfd_seat: %d\n",seat_connection(&connfd)); fflush(stdout);
// 	printf("task_seat: %d\n",seat_connection(&task->connfd)); fflush(stdout);
	pthread_mutex_lock(&pool->queue_lock);
 	printf("ACQUIRED QUEUE LOCK\n"); LINE; fflush(stdout);
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
	printf("RELEASED QUEUE LOCK\n"); fflush(stdout);
	LINE;
	while(sem_wait(pool->sbsem,&pool->sbsem_lock)==-1){usleep(100);}
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
 	free(pool->sbsem);
 	free(pool->seatsem);
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
	int tstr = 0;
	pool_t* p = (pool_t*)pool;
	if(pthread_mutex_lock(&p->queue_lock)==EDEADLK){LINE; printf("DEADLOCKED QUEUE_LOCK\n"); fflush(stdout);}
    while(p->active ==1) {
//    	tstr = 0;
    	//need to try standby list, because someone just cancelled
//    	if(pthread_mutex_trylock(&p->try_sblock)==0){
//     		if(p->trystandbylist==1){
//     			tstr = -1;
// 				p->trystandbylist = 0;
// 				pthread_mutex_unlock(&p->try_sblock);
// 				printf("IN STANDBY LIST\n"); fflush(stdout);
// 				if(pthread_mutex_lock(&p->sblock)==EDEADLK){LINE; printf("DEADLOCKED SBLOCK\n"); fflush(stdout);}
// 			
// 				LINE;
// 				pool_task_t* curr = p->standbylist;
// 				if(curr!=NULL){
// 					LINE;
// 					handle_connection(&curr->connfd,p,p->last_cancelled);
// 					p->standbylist = p->standbylist->next;
// 					sem_post(p->sbsem,p,1,&p->sbsem_lock);
// 				}
// 				printf("LEAVING STANDBY LIST\n"); fflush(stdout);
// 				pthread_mutex_unlock(&p->sblock);
// 			}
// 			pthread_mutex_unlock(&p->try_sblock);
//    	}
//    	else{ pthread_mutex_unlock(&p->try_sblock); }
//     	if(tstr==0){
//     		LINE;
		printf("RELEASING QUEUE LOCK\n"); fflush(stdout);
		sem_post(p->sbsem,p,1,&p->sbsem_lock);
		pthread_cond_wait(&p->notify,&p->queue_lock);
		//sem_wait(p->sbsem,&p->sbsem_lock);
		//if(pthread_cond_wait(&p->notify,&p->queue_lock)==EINVAL){LINE; printf("QUEUE_LOCK NOT OWNED WHEN PTHREAD_COND_WAIT IS CALLED\n"); fflush(stdout);} //Release and acquire lock
		printf("ACQUIRED QUEUE LOCK\n"); fflush(stdout);
		LINE;
	//	printf("About to handle connection\n"); fflush(stdout);
		if(p->queue!=NULL){
			//printf("connfd: %d\n",p->queue->connfd); fflush(stdout);
			int* cfd = &p->queue->connfd;
			p->queue = p->queue->next;
			pthread_mutex_unlock(&p->queue_lock);
			handle_connection(cfd,p,-1);
			pthread_mutex_lock(&p->queue_lock);
		}
		printf("Handled connection\n"); fflush(stdout);
    }
    pthread_exit(NULL);
    return(NULL);
}
