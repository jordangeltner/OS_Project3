#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#include "util.h"

int sem_wait(m_sem_t *s)
{
   	printf("SEM_WAIT: %d\n",s->value); fflush(stdout);
    if(s->value>0){
    	s->value--;
    	return 0;
    }
    else{
    	return -1;
    }
}

int sem_post(m_sem_t *s, pool_t* p)
{
	printf("SEM_POST: %d\n",s->value); fflush(stdout);
    s->value++;
    if(s->value<=0){
    	pthread_cond_signal(&p->sbnotify);
    }
    return 0;
}
