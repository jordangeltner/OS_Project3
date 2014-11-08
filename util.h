#ifndef _UTIL_H_
#define _UTIL_H_


#include <pthread.h>
#include <stdio.h>

#define NUM_SEATS 20 //CHECK WITH PIAZZA
#define LINE printf("LINE: %d\n", __LINE__)

typedef struct m_sem_t {
    int value;
} m_sem_t;

typedef struct pool_task_t {
    int seat_id;
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
  pthread_mutex_t sblock;
  pthread_cond_t sbnotify;
  m_sem_t* sbsem;
  m_sem_t* seatsem;
  int trystandbylist; //0 or 1 that indicates whether or not we need to try tasks 
  						// from the standby list
  int last_cancelled;
  pthread_mutex_t cancellock;
  pthread_mutex_t try_sblock;
  int thread_count;
  int task_queue_size_limit;
  int active;
} pool_t;


void handle_connection(int*,pool_t*);
int sem_wait(m_sem_t *s);
int sem_post(m_sem_t *s, pool_t* p);





#endif
