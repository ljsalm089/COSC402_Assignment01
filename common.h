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

#define __DEBUG__ 1
#ifdef __DEBUG__

#include <time.h>

static time_t now;
static struct tm* c;

#define TIME() time(&now) && (c = localtime(&now)) \
                    && printf("%d-%02d-%02d %02d:%02d:%02d  ", c->tm_year + 1900, c->tm_mon + 1, c->tm_mday, \
                    c->tm_hour, c->tm_min, c->tm_sec)

#define LOG_PREFIX(l, t) TIME() && printf("%s %s\t\t", l, t)

#define I(t_, ...) LOG_PREFIX("I", t_) && LOG("I\t") && printf(__VA_ARGS__) && printf("\n")
#define D(t_, ...) LOG_PREFIX("D", t_) && printf(__VA_ARGS__) && printf("\n")
#define E(t_, ...) LOG_PREFIX("E", t_) && printf(__VA_ARGS__) && printf("\n")

#define DEBUG_BLOCK(x) x

#else

#define I(t_, ...) 
#define D(t_, ...)
#define E(t_, ...)

#define DEBUG_BLOCK(x)

#endif // __DEBUG__

#define err_sys(t_, ...) E((t_), __VA_ARGS__); fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); exit(1)

#define SUCC 0
#define FAIL -1

#define max(x, y) (x) > (y) ? (x) : (y)
#define min(x, y) (x) < (y) ? (x) : (y)

#endif // __COMMON_H__
