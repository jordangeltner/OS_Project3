#ifndef _UTIL_H_
#define _UTIL_H_


#include <pthread.h>
#include <stdio.h>
#include <errno.h>

#define NUM_SEATS 20 //CHECK WITH PIAZZA
#define LINE printf("LINE: %d\n", __LINE__)
#define BUFSIZE 8192

typedef enum 
{
    AVAILABLE, 
    PENDING, 
    OCCUPIED
} seat_state_t;

typedef struct seat_struct
{
    int id;
    int customer_id;
    seat_state_t state;
    struct seat_struct* next;
} seat_t;

typedef struct m_sem_t {
    int value;
} m_sem_t;


typedef struct pool_task_t {
    int seat_id;
    int connfd;
    void *next; //Used to link up queue
    int priority;
    int user_id;
    char buf[BUFSIZE+1];
    char resource[101];
    int length;
} pool_task_t;

typedef struct pool_t {
  pthread_mutex_t queue_lock; //protects the worker queue
  pthread_mutex_t seat_locks[NUM_SEATS];
  pthread_cond_t notify;
  pthread_cond_t task_notify;
  pthread_t *threads;
  pool_task_t *queue;
  pool_task_t *standbylist; //The standby list
  pthread_mutex_t sblock;
  pthread_cond_t sbnotify;
  m_sem_t* sbsem;
  m_sem_t* seatsem;
  m_sem_t* threadsem;
  int trystandbylist; //0 or 1 that indicates whether or not we need to try tasks 
  						// from the standby list
  int last_cancelled;
  pthread_mutex_t cancellock;
  pthread_mutex_t try_sblock;
  pthread_mutex_t sbsem_lock;
  pthread_mutex_t threadsem_lock;
  pthread_mutex_t seatsem_lock;
  int thread_count;
  int task_queue_size_limit;
  int active;
  pthread_t sbtid;
} pool_t;

//int seat_connection(int*);
void setup_connection(int*,pool_t*,pool_task_t*);
//void handle_connection(int*,pool_t*);
void load_connection(pool_task_t*,pool_t*);
int sem_wait(m_sem_t *s, pthread_mutex_t*);
int sem_wait_check(m_sem_t *s, pthread_mutex_t*);
int sem_post(m_sem_t *s,pthread_mutex_t*);





#endif
