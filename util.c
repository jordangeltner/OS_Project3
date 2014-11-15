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
                    //this is an \r\n endline for request
                    //we want to then return the line
                    //readnbytes(fd, &c, 1);
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
    
    //(seatnum==0) ? -1 : seatnum;
}


void setup_connection(int* connfd_ptr, pool_t* p, pool_task_t* task)
{
    int connfd = *(connfd_ptr);
    int fd;
    char buf[BUFSIZE+1];
    char instr[20];
    char file[100];
    char type[20];

    int i=0;
    int j=0;

    char *ok_response = "HTTP/1.0 200 OK\r\n"\
                           "Content-type: text/html\r\n\r\n";

    char *notok_response = "HTTP/1.0 404 FILE NOT FOUND\r\n"\
                            "Content-type: text/html\r\n\r\n"\
                            "<html><body bgColor=white text=black>\n"\
                            "<h2>404 FILE NOT FOUND</h2>\n"\
                            "</body></html>\n";

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

    char *bad_request = "HTTP/1.0 400 BAD REQUEST\r\n"\
                              "Content-type: text/html\r\n\r\n"\
                              "<html><body><h2>BAD REQUEST</h2>"\
                              "</body></html>\n";
                              
    if (strncmp(resource, "list_seats", length) == 0)
    {  
    	printf("List seats\n"); fflush(stdout);
        list_seats(buf, BUFSIZE, p);
        // send headers
        writenbytes(connfd, ok_response, strlen(ok_response));
        // send data
        writenbytes(connfd, buf, strlen(buf));
    } 
    else if(strncmp(resource, "view_seat", length) == 0)
    {	
    	printf("View seat: %d\n", seat_id); fflush(stdout);
    	LINE;
    	if(sem_wait(p->seatsem,&p->seatsem_lock)!=-1){
    		LINE;
    		if(seat_id<20 && seat_id>-1){  		
    			pthread_mutex_lock(&p->seat_locks[seat_id]);
    		}
    		LINE;
    		printf("Viewing seat %d\n",seat_id); fflush(stdout);
			view_seat(buf, BUFSIZE, seat_id, user_id, customer_priority);
			// send headers
			writenbytes(connfd, ok_response, strlen(ok_response));
			// send data
			writenbytes(connfd, buf, strlen(buf));
			if(seat_id<20 && seat_id>-1){
				pthread_mutex_unlock(&p->seat_locks[seat_id]);
			}
		}
		else{
			//space is available on standbylist
			seat_t* seat = get_seat(seat_id);
			LINE;
			if(sem_wait(p->sbsem,&p->sbsem_lock)!=-1 && seat->state==PENDING){
				LINE;
				pthread_mutex_lock(&p->sblock);
				printf("ACQUIRED STANDBY LOCK %d\n",pthread_self()); LINE; fflush(stdout);
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
				printf("RELEASED STANDBY LOCK %d\n",pthread_self()); LINE; fflush(stdout);
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
    	printf("Confirm seat: %d\n", seat_id); fflush(stdout);
    	LINE;
    	pthread_mutex_lock(&p->seat_locks[seat_id]);
    	LINE;
        confirm_seat(buf, BUFSIZE, seat_id, user_id, customer_priority);
        pthread_mutex_unlock(&p->seat_locks[seat_id]);
        // send headers
        writenbytes(connfd, ok_response, strlen(ok_response));
        // send data
        writenbytes(connfd, buf, strlen(buf));
        LINE;
        pthread_mutex_lock(&p->sblock);
        printf("ACQUIRED STANDBY LOCK %d\n",pthread_self()); LINE; fflush(stdout);
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
        		printf("RELEASED STANDBY LOCK %d\n",pthread_self()); LINE; fflush(stdout);
        		load_connection(curr,p);
        		LINE;
        		pthread_mutex_lock(&p->sblock);
        		printf("ACQUIRED STANDBY LOCK %d\n",pthread_self()); LINE; fflush(stdout);
        	}
        	else{
        		prev = curr;
        	}
        	curr = curr->next;
        }
        pthread_mutex_unlock(&p->sblock);
        printf("RELEASED STANDBY LOCK %d\n",pthread_self()); LINE; fflush(stdout);
        int i;
        for(i=0;i<spaces;i++){
        	sem_post(p->sbsem,&p->sbsem_lock);
        }
    }
    else if(strncmp(resource, "cancel", length) == 0)
    {
    	printf("Cancel seat: %d\n", seat_id); fflush(stdout);
    	if(pthread_mutex_lock(&p->seat_locks[seat_id])==EDEADLK){ printf("DEADLOCKED THREAD\n"); fflush(stdout);}
        cancel(buf, BUFSIZE, seat_id, user_id, customer_priority);
        pthread_mutex_unlock(&p->seat_locks[seat_id]);
        // send headers
        writenbytes(connfd, ok_response, strlen(ok_response));
        // send data
        writenbytes(connfd, buf, strlen(buf));
        LINE;
        if(pthread_mutex_lock(&p->cancellock)==EDEADLK){ printf("DEADLOCKED THREAD\n"); fflush(stdout);}
        LINE;
        p->last_cancelled = seat_id;
        p->sbtid = pthread_self();
        
        sem_post(p->seatsem,&p->seatsem_lock);
    }
    else
    {
    	printf("OPENING FILE: %s\n", resource); fflush(stdout);
        // try to open the file
        if ((fd = open(resource, O_RDONLY)) == -1)
        {
            writenbytes(connfd, notok_response, strlen(notok_response));
        } 
        else
        {
            // send headers
            writenbytes(connfd, ok_response, strlen(ok_response));
            // send file
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


// void handle_connection(int* connfd_ptr, pool_t* p)
// {
// 	LINE;
//     int connfd = *(connfd_ptr);
//     int fd;
//     char buf[BUFSIZE+1];
//     char instr[20];
//     char file[100];
//     char type[20];
// 
//     int i=0;
//     int j=0;
// 
//     char *ok_response = "HTTP/1.0 200 OK\r\n"\
//                            "Content-type: text/html\r\n\r\n";
// 
//     char *notok_response = "HTTP/1.0 404 FILE NOT FOUND\r\n"\
//                             "Content-type: text/html\r\n\r\n"\
//                             "<html><body bgColor=white text=black>\n"\
//                             "<h2>404 FILE NOT FOUND</h2>\n"\
//                             "</body></html>\n";
// 
//     char *bad_request = "HTTP/1.0 400 BAD REQUEST\r\n"\
//                               "Content-type: text/html\r\n\r\n"\
//                               "<html><body><h2>BAD REQUEST</h2>"\
//                               "</body></html>\n";
//                               
// 
//     // first read loop -- get request and headers
// 
//     // parse request to get file name
//     // Assumption: this is a GET request and filename contains no spaces
//     // parse request
//     // get headers
// 
//     //Expection Format: 'GET filenane.txt HTTP/1.X'
//     LINE;
//     get_line(connfd, buf, BUFSIZE);
//     LINE;
//     //parse out instruction
//     while( !isspace(buf[j]) && (i < sizeof(instr) - 1))
//     {
//         instr[i] = buf[i];
//         i++;
//         j++;
//     }
//     j+=2;
//     instr[i] = '\0';
// 
// 
//     //Only accept GET requests
//     if (strncmp(instr, "GET", 3) != 0) {
//         writenbytes(connfd, bad_request, strlen(bad_request));
//         close(connfd);
//         return;
//     }
// 
//     //parse out filename
//     i=0;
//     while (!isspace(buf[j]) && (i < sizeof(file) - 1))
//     {
//         file[i] = buf[j];
//         i++;
//         j++;
//     }
//     j++;
//     file[i] = '\0';
// 
//     //parse out type
//     i=0;
//     while (!isspace(buf[j]) && (buf[j] != '\0') && (i < sizeof(type) - 1))
//     {
//         type[i] = buf[j];
//         i++;
//         j++;
//     }
//     type[i] = '\0';
// 
//     while (get_line(connfd, buf, BUFSIZE) > 0)
//     {
//         //ignore headers -> (for now)
//     }
// 
//     int length;
//     for(i = 0; i < strlen(file); i++)
//     {
//         if(file[i] == '?')
//             break;
//     }
//     length = i;
//     
//     char resource[length+1];
// 
//     if (length > strlen(file)) {
//       length = strlen(file);
//     }
//     strncpy(resource, file, length);
//     resource[length] = 0;
//     int seat_id = parse_int_arg(file, "seat=");
//     int user_id = parse_int_arg(file, "user=");
//     int customer_priority = parse_int_arg(file, "priority=");
//     if (strncmp(resource, "list_seats", length) == 0)
//     {  
//     	printf("List seats\n"); fflush(stdout);
//         list_seats(buf, BUFSIZE, p);
//         // send headers
//         writenbytes(connfd, ok_response, strlen(ok_response));
//         // send data
//         writenbytes(connfd, buf, strlen(buf));
//     } 
//     else if(strncmp(resource, "view_seat", length) == 0)
//     {	
//     	printf("View seat: %d\n", seat_id); fflush(stdout);
//     	if(sem_wait(p->seatsem,&p->seatsem_lock)!=-1){
//     		if(seat_id<20 && seat_id>-1){  		
//     			pthread_mutex_lock(&p->seat_locks[seat_id]);
//     		}
//     		printf("Viewing seat %d\n",seat_id); fflush(stdout);
// 			view_seat(buf, BUFSIZE, seat_id, user_id, customer_priority);
// 			// send headers
// 			writenbytes(connfd, ok_response, strlen(ok_response));
// 			// send data
// 			writenbytes(connfd, buf, strlen(buf));
// 			if(seat_id<20 && seat_id>-1){
// 				pthread_mutex_unlock(&p->seat_locks[seat_id]);
// 			}
// 		}
// 		else{
// 			snprintf(buf, BUFSIZE, "Seat unavailable\n\n");
// 			writenbytes(connfd, ok_response, strlen(ok_response));
// 			writenbytes(connfd, buf, strlen(buf));
// 		}
//     } 
//     else if(strncmp(resource, "confirm", length) == 0)
//     {
//     	printf("Confirm seat: %d\n", seat_id); fflush(stdout);
//     	pthread_mutex_lock(&p->seat_locks[seat_id]);
//         confirm_seat(buf, BUFSIZE, seat_id, user_id, customer_priority);
//         // send headers
//         writenbytes(connfd, ok_response, strlen(ok_response));
//         // send data
//         writenbytes(connfd, buf, strlen(buf));
//         pthread_mutex_unlock(&p->seat_locks[seat_id]);
//     }
//     else if(strncmp(resource, "cancel", length) == 0)
//     {
//     	printf("Cancel seat: %d\n", seat_id); fflush(stdout);
//     	if(pthread_mutex_lock(&p->seat_locks[seat_id])==EDEADLK){ printf("DEADLOCKED THREAD\n"); fflush(stdout);}
//     	LINE;
// //     	if(pthread_mutex_lock(&p->cancellock)==EDEADLK){ printf("DEADLOCKED THREAD\n"); fflush(stdout);}
// //     	LINE;
// //     	if(pthread_mutex_lock(&p->try_sblock)==EDEADLK){ printf("DEADLOCKED THREAD\n"); fflush(stdout);}
// //         LINE;
//         cancel(buf, BUFSIZE, seat_id, user_id, customer_priority);
//         // send headers
//         writenbytes(connfd, ok_response, strlen(ok_response));
//         // send data
//         writenbytes(connfd, buf, strlen(buf));
//         p->last_cancelled = seat_id;
//         p->trystandbylist = 1;
//         
// //         pthread_mutex_unlock(&p->try_sblock);
// //         pthread_mutex_unlock(&p->cancellock);
//         pthread_mutex_unlock(&p->seat_locks[seat_id]);
//         //pthread_mutex_unlock(&p->queue_lock);
//         //pthread_cond_signal(&p->notify);
//         sem_post(p->seatsem,&p->seatsem_lock);
//     }
//     else
//     {
//     	printf("OPENING FILE: %s\n", resource); fflush(stdout);
//         // try to open the file
//         if ((fd = open(resource, O_RDONLY)) == -1)
//         {
//             writenbytes(connfd, notok_response, strlen(notok_response));
//         } 
//         else
//         {
//             // send headers
//             writenbytes(connfd, ok_response, strlen(ok_response));
//             // send file
//             int ret;
//             while ( (ret = read(fd, buf, BUFSIZE)) > 0) {
//                 writenbytes(connfd, buf, ret);
//             }  
//             // close file and free space
//             close(fd);
//         } 
//     }
//     close(connfd);
// }