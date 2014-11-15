#ifndef _SEAT_OPERATIONS_H_
#define _SEAT_OPERATIONS_H_

#include "util.h"


void load_seats(int);
void unload_seats();

void list_seats(char* buf, int bufsize, pool_t* p);
void view_seat(char* buf, int bufsize, int seat_num, int customer_num, int customer_priority);
void confirm_seat(char* buf, int bufsize, int seat_num, int customer_num, int customer_priority);
void cancel(char* buf, int bufsize, int seat_num, int customer_num, int customer_priority);

#endif
