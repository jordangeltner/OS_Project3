#include <stdlib.h>
#include <signal.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <errno.h>

#include "util.h"
#include "thread_pool.h"
#include "seats.h"



int writenbytes(int,char *,int);
int readnbytes(int,char *,int);
int get_line(int, char*,int);

int parse_int_arg(char* filename, char* arg);


int get_line(int fd, char *buf, int size)
{

    int i=0;
    char c = '\0';
    int n;

    while((i < size-1) && (c != '\n'))
    {
        n = readnbytes(fd, &c, 1);
        if (n > 0)
        {
            if (c == '\r')
            {
                n = readnbytes(fd, &c, 1);

                if ((n > 0) && (c == '\n'))
                {
                    continue;
                } 
                else 
                {
                    c = '\n';
                }
            }
            buf[i] = c;
            i++;
        } 
        else
        {
            c = '\n';
        }
    }
    buf[i] = '\0';
    return i;
}

int readnbytes(int fd,char *buf,int size)
{
    int rc = 0;
    int totalread = 0;
    while ((rc = read(fd,buf+totalread,size-totalread)) > 0)
        totalread += rc;

    if (rc < 0)
    {
        return -1;
    }
    else
        return totalread;
}

int writenbytes(int fd,char *str,int size)
{
    int rc = 0;
    int totalwritten =0;
    while ((rc = write(fd,str+totalwritten,size-totalwritten)) > 0)
        totalwritten += rc;

    if (rc < 0)
        return -1;
    else
        return totalwritten;
}

int parse_int_arg(char* filename, char* arg)
{
    int i;
    bool found_value_start = false;
    bool found_arg_list_start = false;
    int seatnum = 0;
    for(i=0; i < strlen(filename); i++)
    {
        if (!found_arg_list_start)
        {
            if (filename[i] == '?')
            {
                found_arg_list_start = true;
            }
            continue;
        }
        if (!found_value_start && strncmp(&filename[i], arg, strlen(arg)) == 0)
        {
            found_value_start = true;
            i += strlen(arg);
        }
        if (found_value_start)
        {
            if(isdigit(filename[i]))
            {
                seatnum = seatnum * 10 + (int) filename[i] - (int) '0';
                continue;
            } 
            else
            {
                break;
            }
        }
    }
    return seatnum;
}


void setup_connection(int* connfd_ptr, pool_t* p, pool_task_t* task)
{
    int connfd = *(connfd_ptr);
    char buf[BUFSIZE+1];
    char instr[20];
    char file[100];
    char type[20];

    int i=0;
    int j=0;

    char *bad_request = "HTTP/1.0 400 BAD REQUEST\r\n"\
                              "Content-type: text/html\r\n\r\n"\
                              "<html><body><h2>BAD REQUEST</h2>"\
                              "</body></html>\n";
                              

    // first read loop -- get request and headers

    // parse request to get file name
    // Assumption: this is a GET request and filename contains no spaces
    // parse request
    // get headers

    //Expection Format: 'GET filenane.txt HTTP/1.X'
    get_line(connfd, buf, BUFSIZE);
    //parse out instruction
    while( !isspace(buf[j]) && (i < sizeof(instr) - 1))
    {
        instr[i] = buf[i];
        i++;
        j++;
    }
    j+=2;
    instr[i] = '\0';


    //Only accept GET requests
    if (strncmp(instr, "GET", 3) != 0) {
        writenbytes(connfd, bad_request, strlen(bad_request));
        close(connfd);
        task = NULL;
        return;
    }

    //parse out filename
    i=0;
    while (!isspace(buf[j]) && (i < sizeof(file) - 1))
    {
        file[i] = buf[j];
        i++;
        j++;
    }
    j++;
    file[i] = '\0';

    //parse out type
    i=0;
    while (!isspace(buf[j]) && (buf[j] != '\0') && (i < sizeof(type) - 1))
    {
        type[i] = buf[j];
        i++;
        j++;
    }
    type[i] = '\0';

    while (get_line(connfd, buf, BUFSIZE) > 0)
    {
        //ignore headers -> (for now)
    }

    int length;
    for(i = 0; i < strlen(file); i++)
    {
        if(file[i] == '?')
            break;
    }
    length = i;
    
    char resource[length+1];

    if (length > strlen(file)) {
      length = strlen(file);
    }
    strncpy(resource, file, length);
    resource[length] = 0;
    int seat_id = parse_int_arg(file, "seat=");
    int user_id = parse_int_arg(file, "user=");
    int customer_priority = parse_int_arg(file, "priority=");
	
	//finished initial parsing of file descriptor, update all parts of task
    task->seat_id = seat_id;
    task->user_id = user_id;
    task->priority = customer_priority;
    task->connfd = connfd;
    task->next = NULL;
    task->length = length;
    strncpy(task->buf,buf,BUFSIZE+1);
    strncpy(task->resource,resource,101);
    return;
}

// executes the actual request based on contents of the task
void load_connection(pool_task_t* task, pool_t* p)
{	
	int seat_id = task->seat_id;
	int connfd = task->connfd;
	int customer_priority = task->priority;
	int user_id = task->user_id;
	int length = task->length;
	char* buf = task->buf;
	char* resource = task->resource;
	int fd;
    char *ok_response = "HTTP/1.0 200 OK\r\n"\
                           "Content-type: text/html\r\n\r\n";

    char *notok_response = "HTTP/1.0 404 FILE NOT FOUND\r\n"\
                            "Content-type: text/html\r\n\r\n"\
                            "<html><body bgColor=white text=black>\n"\
                            "<h2>404 FILE NOT FOUND</h2>\n"\
                            "</body></html>\n";
                              
    if (strncmp(resource, "list_seats", length) == 0)
    {  
        list_seats(buf, BUFSIZE, p);
        // send headers
        writenbytes(connfd, ok_response, strlen(ok_response));
        // send data
        writenbytes(connfd, buf, strlen(buf));
    } 
    else if(strncmp(resource, "view_seat", length) == 0)
    {	
		//if there are seats left
    	if(sem_wait(p->seatsem,&p->seatsem_lock)!=-1){
			//get the lock on the seat
    		if(seat_id<20 && seat_id>-1){  		
    			pthread_mutex_lock(&p->seat_locks[seat_id]);
    		}
			view_seat(buf, BUFSIZE, seat_id, user_id, customer_priority);
			writenbytes(connfd, ok_response, strlen(ok_response));
			writenbytes(connfd, buf, strlen(buf));
			if(seat_id<20 && seat_id>-1){
				pthread_mutex_unlock(&p->seat_locks[seat_id]);
			}
		}
		else{
			//space is available on standbylist
			seat_t* seat = get_seat(seat_id);
			
			//only add to standby list if the seat is pending
			if(sem_wait(p->sbsem,&p->sbsem_lock)!=-1 && seat->state==PENDING){
				pthread_mutex_lock(&p->sblock);
				pool_task_t* prev = NULL;
				pool_task_t* curr = p->standbylist;
				while(curr!=NULL){
					prev = curr;
					curr = curr->next;
				}
				if(prev!=NULL){
					prev->next = task;
				}
				else{
					p->standbylist = task;
				}
				task->next = NULL;
				pthread_mutex_unlock(&p->sblock);
				return;
			}
			//no space available on standbylist
			else{
				snprintf(buf, BUFSIZE, "Seat unavailable\n\n");
				writenbytes(connfd, ok_response, strlen(ok_response));
				writenbytes(connfd, buf, strlen(buf));
			}	
		}
    } 
    else if(strncmp(resource, "confirm", length) == 0)
    {
    	pthread_mutex_lock(&p->seat_locks[seat_id]);
        confirm_seat(buf, BUFSIZE, seat_id, user_id, customer_priority);		
        pthread_mutex_unlock(&p->seat_locks[seat_id]);
		
        writenbytes(connfd, ok_response, strlen(ok_response));
        writenbytes(connfd, buf, strlen(buf));
		
		//check the standby list for requests that waited for this seat
        pthread_mutex_lock(&p->sblock);
        pool_task_t* curr = p->standbylist;
        pool_task_t* prev = NULL;
        int spaces = 0;
        while(curr!=NULL){
        	if(curr->seat_id==seat_id){
        		spaces++;
        		if(prev==NULL){
        			p->standbylist = p->standbylist->next;
        		}
        		else{
        			prev->next = curr->next;
        		}
        		pthread_mutex_unlock(&p->sblock);
        		load_connection(curr,p);
        		pthread_mutex_lock(&p->sblock);
        	}
        	else{
        		prev = curr;
        	}
        	curr = curr->next;
        }
        pthread_mutex_unlock(&p->sblock);
        int i;
        for(i=0;i<spaces;i++){
        	sem_post(p->sbsem,&p->sbsem_lock);
        }
    }
    else if(strncmp(resource, "cancel", length) == 0)
    {
    	if(pthread_mutex_lock(&p->seat_locks[seat_id])==EDEADLK){ printf("DEADLOCKED THREAD\n"); fflush(stdout);}
        cancel(buf, BUFSIZE, seat_id, user_id, customer_priority);
        pthread_mutex_unlock(&p->seat_locks[seat_id]);
		
        writenbytes(connfd, ok_response, strlen(ok_response));
        writenbytes(connfd, buf, strlen(buf));
		
        if(pthread_mutex_lock(&p->cancellock)==EDEADLK){ printf("DEADLOCKED THREAD\n"); fflush(stdout);}
		
		//update the cancellation info in pool struct 
        p->last_cancelled = seat_id;
        p->sbtid = pthread_self();
        
        sem_post(p->seatsem,&p->seatsem_lock);
    }
    else
    {
        // try to open the file
        if ((fd = open(resource, O_RDONLY)) == -1)
        {
            writenbytes(connfd, notok_response, strlen(notok_response));
        } 
        else
        {
            writenbytes(connfd, ok_response, strlen(ok_response));
            int ret;
            while ( (ret = read(fd, buf, BUFSIZE)) > 0) {
                writenbytes(connfd, buf, ret);
            }  
            // close file and free space
            close(fd);
        } 
    }
    close(connfd);
}