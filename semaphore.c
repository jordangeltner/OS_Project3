#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#include "util.h"


int sem_wait(m_sem_t *s, pthread_mutex_t* vlock)
{
	pthread_mutex_lock(vlock);
   	printf("SEM_WAIT: %d\n",s->value); fflush(stdout);
    if(s->value>0){
    	s->value--;
    	pthread_mutex_unlock(vlock);
    	return 0;
    }
    else{
    	pthread_mutex_unlock(vlock);
    	return -1;
    }
}
int sem_wait_check(m_sem_t *s, pthread_mutex_t* vlock)
{
	pthread_mutex_lock(vlock);
   	printf("SEM_WAIT: %d\n",s->value); fflush(stdout);
    if(s->value>0){
    	pthread_mutex_unlock(vlock);
    	return 0;
    }
    else{
    	pthread_mutex_unlock(vlock);
    	return -1;
    }
}

int sem_post(m_sem_t *s, pool_t* p, int notify, pthread_mutex_t* vlock)
{
	pthread_mutex_lock(vlock);
	printf("SEM_POST: %d\n",s->value); fflush(stdout);
    s->value++;
    if(notify==0 && s->value>=0){
    	pthread_cond_signal(&p->sbnotify);
    }
    pthread_mutex_unlock(vlock);
//     if(s->value<=0){
//     	if(notify
//     	pthread_cond_signal(&p->sbnotify);
//     }
    return 0;
}
