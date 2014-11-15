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
  pthread_mutex_t seat_locks[NUM_SEATS]; //protects the state of individual seats
  pthread_cond_t notify;   //used to notify threads that work is available
  pthread_t *threads;    
  pool_task_t *queue;  //work queue
  pool_task_t *standbylist; //The standby list
  pthread_mutex_t sblock;  //Protects modification of the standby list
  m_sem_t* sbsem;			//The standby list semaphore
  m_sem_t* seatsem;			//The available seats semaphore
  m_sem_t* threadsem;		//The threads available to take work semaphore
  int last_cancelled;		//Holds the seat_id of the most recently cancelled seat
  							//Set to -1 after the standby list is checked for
  							//serviceable requests of seat_id==last_cancelled
  pthread_t sbtid;			//Holds the thread id of the thread who should handle
  							//checking the standby list for serviceable requests
  pthread_mutex_t cancellock; //Protects modification of last_cancelled and sbtid
  pthread_mutex_t sbsem_lock;		//Protects the standby list semaphore
  pthread_mutex_t threadsem_lock; 	//Protects the threads available to take work semaphore
  pthread_mutex_t seatsem_lock;		//Protects the available seats semaphore
  int active;			//1 indicates that threads should continue to loop 0 indicates
  						//that all should abort
} pool_t;





void setup_connection(int*,pool_t*,pool_task_t*);
void load_connection(pool_task_t*,pool_t*);
int sem_wait(m_sem_t *s, pthread_mutex_t*);
int sem_wait_check(m_sem_t *s, pthread_mutex_t*);
int sem_post(m_sem_t *s,pthread_mutex_t*);
seat_t * get_seat(int);





#endif
