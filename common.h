#ifndef __COMMON_H__
#define __COMMON_H__

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <error.h>

#ifndef NULL
#define NULL ((void *)0)
#endif // NULL

#ifndef null
#define null ((void *)0)
#endif // null


#define LOG(...) printf(__VA_ARGS__)

#include <time.h>

static time_t now;
static struct tm* c;

// print out the time for each line of log
#define TIME() time(&now) && (c = localtime(&now)) \
                    && printf("%02d-%02d %02d:%02d:%02d  ", c->tm_mon + 1, c->tm_mday, \
                    c->tm_hour, c->tm_min, c->tm_sec)
#define LOG_PREFIX(l, t) TIME() && printf("%s %-15s", l, t)

// #define __DEBUG__ 1
#ifdef __DEBUG__

// uncomment the line below to enable log to detect deadlock
// #define __DEBUG_LOCK__ 1


// macro to log out to stdout
#define D(t_, ...) LOG_PREFIX("D", t_) && printf(__VA_ARGS__) && printf("\n")
#define I(t_, ...) LOG_PREFIX("I", t_) && printf(__VA_ARGS__) && printf("\n")
#define E(t_, ...) LOG_PREFIX("E", t_) && printf(__VA_ARGS__) && printf("\n")

#define DEBUG_BLOCK(x) x

#else // __DEBUG__

#define I(t_, ...) LOG_PREFIX("I", t_) && printf(__VA_ARGS__) && printf("\n")
#define E(t_, ...) LOG_PREFIX("E", t_) && printf(__VA_ARGS__) && printf("\n")
#define D(t_, ...)


#define DEBUG_BLOCK(x)


#endif // __DEBUG__

#define err_sys(t_, ...) E((t_), __VA_ARGS__); fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); exit(1)

#define SUCC 0
#define FAIL -1

#define max(x, y) (x) > (y) ? (x) : (y)
#define min(x, y) (x) < (y) ? (x) : (y)


// use log to detect deadlock
#ifdef __DEBUG_LOCK__

#define LOCK "LOCK"

#define SPIN_LOCK(l, msg) D(LOCK, "Before %lu: " #l " " msg, pthread_self()); \
    pthread_spin_lock((l)); D(LOCK, "After %lu: " #l" " msg, pthread_self()); 
#define SPIN_UNLOCK(l, msg) D(LOCK, "Before %lu: " #l " " msg, pthread_self()); \
    pthread_spin_unlock((l)); D(LOCK, "After %lu: " #l " " msg, pthread_self());

#define MUTEX_LOCK(l, msg) D(LOCK, "Before %lu: " #l " " msg, pthread_self()); \
    pthread_mutex_lock((l)); D(LOCK, "After %lu: " #l " " msg, pthread_self());
#define MUTEX_UNLOCK(l, msg) D(LOCK, "Before %lu: " #l " " msg, pthread_self()); \
    pthread_mutex_unlock((l)); D(LOCK, "After %lu: " #l " " msg, pthread_self());

#else // __DEBUG_LOCK__

#define SPIN_LOCK(l, msg) pthread_spin_lock((l));
#define SPIN_UNLOCK(l, msg) pthread_spin_unlock((l));

#define MUTEX_LOCK(l, msg) pthread_mutex_lock((l));
#define MUTEX_UNLOCK(l, msg) pthread_mutex_unlock((l));

#endif // __DEBUG_LOCK__

#endif // __COMMON_H__
