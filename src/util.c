#include "common.h"

#include <sys/stat.h>

static int parse_long_field(const char *text, long *value)
{
    char *end = NULL;
    long v;

    if (text == NULL || *text == '\0')
        return -1;

    errno = 0;
    v = strtol(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0')
        return -1;

    *value = v;
    return 0;
}

int parse_int_range(const char *text, int min, int max, int *out)
{
    long v;

    if (parse_long_field(text, &v) != 0 || v < min || v > max)
        return -1;

    *out = (int)v;
    return 0;
}

int parse_jobs(const char *spec, Job jobs[], int max_jobs, char *err, size_t err_len)
{
    char *copy;
    char *token;
    char *save = NULL;
    int count = 0;

    if (spec == NULL || *spec == '\0') {
        snprintf(err, err_len, "job specification is empty");
        return -1;
    }
    if (*spec == ',' || spec[strlen(spec) - 1] == ',' ||
        strstr(spec, ",,") != NULL) {
        snprintf(err, err_len, "empty job entry in specification");
        return -1;
    }

    copy = strdup(spec);
    if (copy == NULL) {
        snprintf(err, err_len, "out of memory");
        return -1;
    }

    for (token = strtok_r(copy, ",", &save); token != NULL;
         token = strtok_r(NULL, ",", &save)) {
        char *fields[4];
        char *field_save = NULL;
        char *field;
        int nfields = 0;
        long id, arrival, burst, priority;
        int i;

        if (count >= max_jobs) {
            snprintf(err, err_len, "too many jobs (maximum %d)", max_jobs);
            free(copy);
            return -1;
        }

        for (field = strtok_r(token, ":", &field_save); field != NULL;
             field = strtok_r(NULL, ":", &field_save)) {
            if (nfields == 4) {
                snprintf(err, err_len, "job '%s' has more than 4 fields", token);
                free(copy);
                return -1;
            }
            fields[nfields++] = field;
        }

        if (nfields != 4) {
            snprintf(err, err_len,
                     "job '%s' must have id:arrival:burst:priority", token);
            free(copy);
            return -1;
        }

        if (parse_long_field(fields[0], &id) != 0 || id < 1 || id > MAX_ID) {
            snprintf(err, err_len, "job id '%s' must be an integer 1..%d",
                     fields[0], MAX_ID);
            free(copy);
            return -1;
        }
        if (parse_long_field(fields[1], &arrival) != 0 || arrival < 0 ||
            arrival > MAX_ARRIVAL) {
            snprintf(err, err_len,
                     "arrival time '%s' must be an integer 0..%d",
                     fields[1], MAX_ARRIVAL);
            free(copy);
            return -1;
        }
        if (parse_long_field(fields[2], &burst) != 0 || burst < 1 ||
            burst > MAX_BURST) {
            snprintf(err, err_len, "CPU burst '%s' must be an integer 1..%d",
                     fields[2], MAX_BURST);
            free(copy);
            return -1;
        }
        if (parse_long_field(fields[3], &priority) != 0 || priority < 1 ||
            priority > MAX_PRIORITY) {
            snprintf(err, err_len, "priority '%s' must be an integer 1..%d",
                     fields[3], MAX_PRIORITY);
            free(copy);
            return -1;
        }

        for (i = 0; i < count; i++) {
            if (jobs[i].id == (int)id) {
                snprintf(err, err_len, "duplicate job id %ld", id);
                free(copy);
                return -1;
            }
        }

        jobs[count].id = (int)id;
        jobs[count].arrival = (int)arrival;
        jobs[count].burst = (int)burst;
        jobs[count].priority = (int)priority;
        count++;
    }

    free(copy);

    if (count == 0) {
        snprintf(err, err_len, "no jobs found in specification");
        return -1;
    }

    return count;
}

int load_default_jobs(Job jobs[], int *n)
{
    char err[160];
    int count = parse_jobs(DEFAULT_JOB_SPEC, jobs, MAX_JOBS, err, sizeof(err));

    if (count < 0) {
        fprintf(stderr, "internal error: bad default job set: %s\n", err);
        return -1;
    }

    *n = count;
    return 0;
}

int ensure_log_dir(void)
{
    if (mkdir(LOG_DIR, 0755) == 0 || errno == EEXIST)
        return 0;

    fprintf(stderr, "error: cannot create %s: %s\n", LOG_DIR, strerror(errno));
    return -1;
}

void format_timestamp(char *buf, size_t len)
{
    struct timespec ts;
    struct tm tm;

    clock_gettime(CLOCK_REALTIME, &ts);
    localtime_r(&ts.tv_sec, &tm);
    snprintf(buf, len, "%02d:%02d:%02d.%03ld", tm.tm_hour, tm.tm_min,
             tm.tm_sec, ts.tv_nsec / 1000000L);
}

void print_rule(const char *title)
{
    printf("\n=== %s ===\n", title);
}
