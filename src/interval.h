#ifndef INTERVAL_H
#define INTERVAL_H

#ifdef __cplusplus
extern "C" {
#endif

int set_interval(void (*func)(void *arg), int delay, void *arg);

void clear_interval(int id);

double time_precise_ms();

#ifdef __cplusplus
}
#endif

#endif