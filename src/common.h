#ifndef CAMPUSCOMPUTE_COMMON_H
#define CAMPUSCOMPUTE_COMMON_H

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define MAX_JOBS 12
#define MAX_ID 9999
#define MAX_ARRIVAL 10000
#define MAX_BURST 100
#define MAX_PRIORITY 99
#define DEFAULT_QUANTUM 4
#define DEFAULT_JOB_SPEC "1:0:8:2,2:1:4:1,3:2:9:4,4:3:5:2,5:4:2:3"
#define LOG_DIR "logs"
#define LOG_FILE "logs/campuscompute.log"

typedef struct {
    int id;
    int arrival;
    int burst;
    int priority;
} Job;

/* Parses "id:arrival:burst:priority,..." and validates every field. */
int parse_jobs(const char *spec, Job jobs[], int max_jobs, char *err, size_t err_len);
int load_default_jobs(Job jobs[], int *n);
int parse_int_range(const char *text, int min, int max, int *out);
int ensure_log_dir(void);
void format_timestamp(char *buf, size_t len);
void print_rule(const char *title);

#endif
