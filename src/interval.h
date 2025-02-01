#ifndef INTERVAL_H
#define INTERVAL_H

int set_interval(void (*func)(void *arg), int delay, void *arg);

void clear_interval(int id);


#endif