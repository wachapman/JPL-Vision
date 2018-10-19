#ifndef GET_USECS_H
#define GET_USECS_H

#include <time.h>
#include <stdint.h>
#include <string.h>
#include "log_error.h"

#define USECS_PER_SECOND 1000000

static inline int64_t get_usecs() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * (int64_t)1000000 + ts.tv_nsec / (int64_t)1000;
}

static inline usecs_init() {
    char* endp;
    char buf[26];
    int64_t usecs = get_usecs();
    time_t t = time(NULL);
    ctime_r(&t, buf);
    if ((endp = strchr(buf, '\n')) != NULL) *endp = '\0';
    LOG_STATUS("start up at %s; usecs= %lld\n", buf, usecs);
}
#endif
