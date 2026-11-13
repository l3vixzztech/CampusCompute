#include "jobs.h"

static int cmp_arrival_id(const void *a, const void *b)
{
    const Job *ja = a;
    const Job *jb = b;

    if (ja->arrival != jb->arrival)
        return ja->arrival - jb->arrival;
    return ja->id - jb->id;
}

const char *policy_name(SchedPolicy policy)
{
    switch (policy) {
    case POLICY_FCFS:
        return "FCFS";
    case POLICY_RR:
        return "Round Robin";
    case POLICY_PRIORITY:
        return "Priority (non-preemptive)";
    }
    return "Unknown";
}

int policy_parse(const char *name, SchedPolicy *out)
{
    if (strcmp(name, "fcfs") == 0)
        *out = POLICY_FCFS;
    else if (strcmp(name, "rr") == 0)
        *out = POLICY_RR;
    else if (strcmp(name, "priority") == 0)
        *out = POLICY_PRIORITY;
    else
        return -1;
    return 0;
}

static void finish_job(JobResult *r, int completion)
{
    r->completion = completion;
    r->turnaround = completion - r->arrival;
    r->waiting = r->turnaround - r->burst;
}

static void add_segment(ScheduleRun *run, int job_id, int start, int end)
{
    if (end <= start)
        return;
    if (run->nsegs >= MAX_SEGMENTS) {
        fprintf(stderr, "warning: timeline truncated at %d segments\n",
                MAX_SEGMENTS);
        return;
    }
    run->segs[run->nsegs].job_id = job_id;
    run->segs[run->nsegs].start = start;
    run->segs[run->nsegs].end = end;
    run->nsegs++;
}

static void run_fcfs(const Job jobs[], int n, ScheduleRun *run)
{
    Job sorted[MAX_JOBS];
    int time = 0;
    int i;

    memcpy(sorted, jobs, sizeof(Job) * (size_t)n);
    qsort(sorted, (size_t)n, sizeof(Job), cmp_arrival_id);

    for (i = 0; i < n; i++) {
        int idx = -1;
        int j;

        for (j = 0; j < n; j++) {
            if (run->result[j].id == sorted[i].id) {
                idx = j;
                break;
            }
        }
        if (idx < 0)
            continue;

        if (jobs[idx].arrival > time)
            add_segment(run, 0, time, jobs[idx].arrival);
        if (time < jobs[idx].arrival)
            time = jobs[idx].arrival;

        run->result[idx].start = time;
        add_segment(run, jobs[idx].id, time, time + jobs[idx].burst);
        time += jobs[idx].burst;
        finish_job(&run->result[idx], time);
    }
}

static void run_priority(const Job jobs[], int n, ScheduleRun *run)
{
    int done[MAX_JOBS] = {0};
    int completed = 0;
    int time = 0;

    while (completed < n) {
        int best = -1;
        int next_arrival = INT_MAX;
        int i;

        for (i = 0; i < n; i++) {
            if (done[i])
                continue;
            if (jobs[i].arrival <= time) {
                if (best < 0 ||
                    jobs[i].priority < jobs[best].priority ||
                    (jobs[i].priority == jobs[best].priority &&
                     jobs[i].arrival < jobs[best].arrival) ||
                    (jobs[i].priority == jobs[best].priority &&
                     jobs[i].arrival == jobs[best].arrival &&
                     jobs[i].id < jobs[best].id)) {
                    best = i;
                }
            } else if (jobs[i].arrival < next_arrival) {
                next_arrival = jobs[i].arrival;
            }
        }

        if (best < 0) {
            add_segment(run, 0, time, next_arrival);
            time = next_arrival;
            continue;
        }

        run->result[best].start = time;
        add_segment(run, jobs[best].id, time, time + jobs[best].burst);
        time += jobs[best].burst;
        finish_job(&run->result[best], time);
        done[best] = 1;
        completed++;
    }
}

static void run_rr(const Job jobs[], int n, int quantum, ScheduleRun *run)
{
    Job sorted[MAX_JOBS];
    int index_of_id[MAX_ID + 1];
    int remaining[MAX_JOBS];
    int ready[MAX_READY];
    int head = 0, tail = 0, enqueued = 0, completed = 0;
    int time = 0;
    int i;

    if (quantum < 1)
        quantum = 1;

    memcpy(sorted, jobs, sizeof(Job) * (size_t)n);
    qsort(sorted, (size_t)n, sizeof(Job), cmp_arrival_id);

    for (i = 0; i <= MAX_ID; i++)
        index_of_id[i] = -1;
    for (i = 0; i < n; i++) {
        int idx = -1;
        int j;

        for (j = 0; j < n; j++) {
            if (jobs[j].id == sorted[i].id) {
                idx = j;
                break;
            }
        }
        index_of_id[sorted[i].id] = idx;
        remaining[idx] = sorted[i].burst;
    }

    time = sorted[0].arrival;

    while (completed < n) {
        while (enqueued < n && sorted[enqueued].arrival <= time) {
            if (tail >= MAX_READY) {
                fprintf(stderr, "error: round-robin ready queue overflow\n");
                return;
            }
            ready[tail++] = index_of_id[sorted[enqueued].id];
            enqueued++;
        }

        if (head == tail) {
            if (enqueued < n) {
                int next = sorted[enqueued].arrival;

                add_segment(run, 0, time, next);
                time = next;
                continue;
            }
            break;
        }

        {
            int idx = ready[head++];
            int slice = remaining[idx] < quantum ? remaining[idx] : quantum;
            int start = time;

            if (run->result[idx].start < 0)
                run->result[idx].start = start;

            time += slice;
            remaining[idx] -= slice;
            add_segment(run, jobs[idx].id, start, time);

            /* Jobs that arrived during the slice join before the preempted one. */
            while (enqueued < n && sorted[enqueued].arrival <= time) {
                if (tail >= MAX_READY) {
                    fprintf(stderr, "error: round-robin ready queue overflow\n");
                    return;
                }
                ready[tail++] = index_of_id[sorted[enqueued].id];
                enqueued++;
            }

            if (remaining[idx] == 0) {
                finish_job(&run->result[idx], time);
                completed++;
            } else {
                if (tail >= MAX_READY) {
                    fprintf(stderr, "error: round-robin ready queue overflow\n");
                    return;
                }
                ready[tail++] = idx;
            }
        }
    }

    if (completed != n)
        fprintf(stderr, "error: round-robin simulation did not complete all jobs\n");
}

int run_schedule(SchedPolicy policy, int quantum, const Job jobs[], int n,
                 ScheduleRun *run)
{
    int i;

    if (n < 1 || n > MAX_JOBS || quantum < 1) {
        fprintf(stderr, "error: invalid scheduling parameters\n");
        return -1;
    }

    memset(run, 0, sizeof(*run));
    run->n = n;
    for (i = 0; i < n; i++) {
        run->result[i].id = jobs[i].id;
        run->result[i].arrival = jobs[i].arrival;
        run->result[i].burst = jobs[i].burst;
        run->result[i].priority = jobs[i].priority;
        run->result[i].start = -1;
    }

    switch (policy) {
    case POLICY_FCFS:
        run_fcfs(jobs, n, run);
        break;
    case POLICY_RR:
        run_rr(jobs, n, quantum, run);
        break;
    case POLICY_PRIORITY:
        run_priority(jobs, n, run);
        break;
    default:
        fprintf(stderr, "error: unsupported scheduling policy\n");
        return -1;
    }

    return 0;
}

static void print_timeline(const ScheduleRun *run)
{
    int i;
    int column = 10;

    printf("Execution timeline:\n          ");
    for (i = 0; i < run->nsegs; i++) {
        char cell[40];
        int len;

        if (run->segs[i].job_id == 0)
            len = snprintf(cell, sizeof(cell), "| idle %d-%d ",
                           run->segs[i].start, run->segs[i].end);
        else
            len = snprintf(cell, sizeof(cell), "| J%d %d-%d ",
                           run->segs[i].job_id, run->segs[i].start,
                           run->segs[i].end);

        if (column + len > 110) {
            printf("\n          ");
            column = 10;
        }
        printf("%s", cell);
        column += len;
    }
    printf("|\n");
}

void print_schedule(const char *title, SchedPolicy policy, int quantum,
                    const ScheduleRun *run)
{
    double total_wait = 0.0, total_tat = 0.0;
    int makespan = 0;
    int i;

    print_rule(title);
    printf("Policy: %s", policy_name(policy));
    if (policy == POLICY_RR)
        printf(" | Time quantum: %d", quantum);
    printf(" | Jobs: %d\n", run->n);

    if (policy == POLICY_FCFS)
        printf("Tie-break: earlier arrival first, then smaller job ID.\n");
    else if (policy == POLICY_RR)
        printf("Tie-break: FIFO ready queue; simultaneous arrivals by job ID; quantum %d.\n",
               quantum);
    else
        printf("Tie-break: lower priority number first, then earlier arrival, then smaller job ID.\n");

    print_timeline(run);

    printf("\n%-5s %-8s %-6s %-9s %-7s %-11s %-6s %-8s\n",
           "Job", "Arrival", "Burst", "Priority", "Start", "Completion",
           "TAT", "Waiting");
    printf("%-5s %-8s %-6s %-9s %-7s %-11s %-6s %-8s\n",
           "---", "-------", "-----", "--------", "-----", "----------",
           "---", "-------");

    for (i = 0; i < run->n; i++) {
        const JobResult *r = &run->result[i];

        printf("%-5d %-8d %-6d %-9d %-7d %-11d %-6d %-8d\n", r->id,
               r->arrival, r->burst, r->priority, r->start, r->completion,
               r->turnaround, r->waiting);
        total_wait += r->waiting;
        total_tat += r->turnaround;
        if (r->completion > makespan)
            makespan = r->completion;
    }

    printf("\nAverage waiting time:    %6.2f\n", total_wait / run->n);
    printf("Average turnaround time: %6.2f\n", total_tat / run->n);
    printf("Makespan (last completion): %d\n", makespan);
}

static void completion_order(const ScheduleRun *run, char *buf, size_t len)
{
    int order[MAX_JOBS];
    int i, j;
    size_t used = 0;

    for (i = 0; i < run->n; i++)
        order[i] = i;

    for (i = 0; i < run->n - 1; i++) {
        for (j = 0; j < run->n - i - 1; j++) {
            int a = order[j], b = order[j + 1];

            if (run->result[a].completion > run->result[b].completion ||
                (run->result[a].completion == run->result[b].completion &&
                 run->result[a].id > run->result[b].id)) {
                order[j] = b;
                order[j + 1] = a;
            }
        }
    }

    buf[0] = '\0';
    for (i = 0; i < run->n; i++) {
        int n = snprintf(buf + used, len - used, "%sJ%d",
                         i == 0 ? "" : " -> ", run->result[order[i]].id);
        if (n < 0 || (size_t)n >= len - used)
            break;
        used += (size_t)n;
    }
}

int command_compare(const Job jobs[], int n, int quantum)
{
    ScheduleRun runs[3];
    SchedPolicy policies[3] = {POLICY_FCFS, POLICY_RR, POLICY_PRIORITY};
    int i, j;

    if (quantum < 1)
        quantum = DEFAULT_QUANTUM;

    for (i = 0; i < 3; i++) {
        if (run_schedule(policies[i], quantum, jobs, n, &runs[i]) != 0)
            return -1;
    }

    print_rule("Scheduling policy comparison");
    printf("Same job set (%d jobs); Round Robin quantum = %d.\n", n, quantum);
    printf("%-26s %-8s %-12s %-15s %s\n", "Policy", "Quantum",
           "Avg waiting", "Avg turnaround", "Completion order");
    printf("%-26s %-8s %-12s %-15s %s\n", "------", "-------",
           "-----------", "--------------", "----------------");

    for (i = 0; i < 3; i++) {
        double wait = 0.0, tat = 0.0;
        char order[128];
        char qbuf[16];

        for (j = 0; j < n; j++) {
            wait += runs[i].result[j].waiting;
            tat += runs[i].result[j].turnaround;
        }
        completion_order(&runs[i], order, sizeof(order));
        if (policies[i] == POLICY_RR)
            snprintf(qbuf, sizeof(qbuf), "%d", quantum);
        else
            snprintf(qbuf, sizeof(qbuf), "-");
        printf("%-26s %-8s %-12.2f %-15.2f %s\n", policy_name(policies[i]),
               qbuf, wait / n, tat / n, order);
    }

    printf("\nPer-job waiting time:\n");
    printf("%-5s %-14s %-14s %-14s\n", "Job", "FCFS", "RR", "Priority");
    for (j = 0; j < n; j++) {
        printf("%-5d %-14d %-14d %-14d\n", jobs[j].id,
               runs[0].result[j].waiting, runs[1].result[j].waiting,
               runs[2].result[j].waiting);
    }

    printf("\nObservation: lower average waiting time is better for the same job set.\n");
    return 0;
}
