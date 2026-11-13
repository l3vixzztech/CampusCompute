#ifndef CAMPUSCOMPUTE_JOBS_H
#define CAMPUSCOMPUTE_JOBS_H

#include "common.h"

#define MAX_SEGMENTS 4096
#define MAX_READY 2048

typedef enum {
    POLICY_FCFS,
    POLICY_RR,
    POLICY_PRIORITY
} SchedPolicy;

typedef struct {
    int job_id; /* 0 marks an idle CPU interval */
    int start;
    int end;
} Segment;

typedef struct {
    int id;
    int arrival;
    int burst;
    int priority;
    int start;
    int completion;
    int turnaround;
    int waiting;
} JobResult;

typedef struct {
    JobResult result[MAX_JOBS];
    Segment segs[MAX_SEGMENTS];
    int nsegs;
    int n;
} ScheduleRun;

const char *policy_name(SchedPolicy policy);
int policy_parse(const char *name, SchedPolicy *out);
int run_schedule(SchedPolicy policy, int quantum, const Job jobs[], int n,
                 ScheduleRun *run);
void print_schedule(const char *title, SchedPolicy policy, int quantum,
                    const ScheduleRun *run);
int command_compare(const Job jobs[], int n, int quantum);

#endif
