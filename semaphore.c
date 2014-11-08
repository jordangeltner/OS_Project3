#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#include "util.h"

int sem_wait(m_sem_t *s, pthread_cond_t sbnotify, pthread_mutex_t sblock);
int sem_post(m_sem_t *s, pthread_cond_t sbnotify);

int sem_wait(m_sem_t *s, pthread_cond_t sbnotify, pthread_mutex_t sblock)
{
    s->value--; //make sure these don't overlap
    if(s->value<0){
    	pthread_cond_wait(&sbnotify,&sblock);
    }
    return 0;
}

int sem_post(m_sem_t *s, pthread_cond_t sbnotify)
{
    s->value++; //make sure these don't overlap
    if(s->value<=0){
    	pthread_cond_signal(&sbnotify);
    }
    return 0;
}
