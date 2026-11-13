#include "common.h"

#include "banker.h"
#include "ipc.h"
#include "jobs.h"
#include "limits.h"
#include "logger.h"
#include "worker.h"

#include <dirent.h>
#include <glob.h>
#include <sys/stat.h>
#include <sys/wait.h>

static int load_jobs(const char *spec, Job jobs[], int *n)
{
    char err[192];

    if (spec == NULL)
        return load_default_jobs(jobs, n);

    *n = parse_jobs(spec, jobs, MAX_JOBS, err, sizeof(err));
    if (*n < 0) {
        fprintf(stderr, "error: invalid job specification: %s\n", err);
        return -1;
    }
    return 0;
}

static void print_job_table(const Job jobs[], int n)
{
    int i;

    printf("%-5s %-8s %-6s %-9s\n", "Job", "Arrival", "Burst", "Priority");
    printf("%-5s %-8s %-6s %-9s\n", "---", "-------", "-----", "--------");
    for (i = 0; i < n; i++)
        printf("%-5d %-8d %-6d %-9d\n", jobs[i].id, jobs[i].arrival,
               jobs[i].burst, jobs[i].priority);
}

static int command_jobs(void)
{
    Job jobs[MAX_JOBS];
    int n;

    if (load_default_jobs(jobs, &n) != 0)
        return 1;

    print_rule("Built-in job set");
    print_job_table(jobs, n);
    printf("\nJob specification: %s\n", DEFAULT_JOB_SPEC);
    printf("Fields: id:arrival:burst:priority (priority 1 is highest).\n");
    return 0;
}

static void print_usage(FILE *out)
{
    fprintf(out,
            "CampusCompute - intelligent job scheduling and resource management\n"
            "\n"
            "Usage: ./campuscompute <command> [options]\n"
            "\n"
            "Commands:\n"
            "  help                              show this help text\n"
            "  jobs                              list the built-in job set\n"
            "  schedule fcfs [jobspec]           first-come, first-served\n"
            "  schedule rr [quantum] [jobspec]   round robin (default quantum 4)\n"
            "  schedule priority [jobspec]       non-preemptive priority\n"
            "  compare [quantum] [jobspec]       compare all three policies\n"
            "  logger [events_per_producer]      threaded producer-consumer logger\n"
            "  ipc                               POSIX shared memory + semaphore\n"
            "  banker [safe|unsafe|both]         Banker's safety algorithm\n"
            "  worker [normal|fail|both]         fork/waitpid worker lifecycle\n"
            "  limits                            RLIMIT_CPU demonstration\n"
            "  demo                              run the integrated demonstration\n"
            "  cleanup                           verify/remove program IPC objects\n"
            "  menu                              interactive menu\n"
            "\n"
            "jobspec: id:arrival:burst:priority[,id:arrival:burst:priority...]\n"
            "Default job set: " DEFAULT_JOB_SPEC "\n"
            "\n"
            "Rules: job id 1..9999 (unique); arrival 0..10000; burst 1..100;\n"
            "priority 1..99 (lower number = higher priority).\n"
            "Tie-breaks: FCFS/Priority use arrival then job id; Priority uses\n"
            "priority first; Round Robin uses a FIFO ready queue (arrival then id).\n");
}

static int command_schedule(int argc, char **argv)
{
    Job jobs[MAX_JOBS];
    ScheduleRun run;
    SchedPolicy policy;
    const char *spec = NULL;
    int quantum = DEFAULT_QUANTUM;
    int n;
    int argi = 3;

    if (argc < 3) {
        fprintf(stderr, "error: schedule requires a policy (fcfs, rr, priority)\n");
        print_usage(stderr);
        return 2;
    }

    if (policy_parse(argv[2], &policy) != 0) {
        fprintf(stderr, "error: unknown scheduling policy '%s'\n", argv[2]);
        fprintf(stderr, "usage: ./campuscompute schedule <fcfs|rr|priority> [quantum] [jobspec]\n");
        return 2;
    }

    if (argc > 3) {
        if (policy == POLICY_RR && strchr(argv[3], ':') == NULL) {
            if (parse_int_range(argv[3], 1, 1000, &quantum) != 0) {
                fprintf(stderr,
                        "error: invalid quantum '%s' (expected integer 1..1000)\n",
                        argv[3]);
                return 2;
            }
        } else {
            spec = argv[3];
        }
        argi = 4;
    }

    if (argc > argi) {
        if (spec != NULL) {
            fprintf(stderr, "error: too many arguments for schedule\n");
            return 2;
        }
        spec = argv[argi];
        argi++;
    }

    if (argc > argi) {
        fprintf(stderr, "error: too many arguments for schedule\n");
        return 2;
    }

    if (load_jobs(spec, jobs, &n) != 0)
        return 2;

    if (run_schedule(policy, quantum, jobs, n, &run) != 0)
        return 1;

    print_schedule("CPU scheduling result", policy, quantum, &run);
    printf("\nJob set used (same input for every policy):\n");
    print_job_table(jobs, n);
    return 0;
}

static int command_compare_cli(int argc, char **argv)
{
    Job jobs[MAX_JOBS];
    const char *spec = NULL;
    int quantum = DEFAULT_QUANTUM;
    int n;
    int argi = 2;

    if (argc > 2) {
        if (strchr(argv[2], ':') != NULL) {
            spec = argv[2];
        } else {
            if (parse_int_range(argv[2], 1, 1000, &quantum) != 0) {
                fprintf(stderr,
                        "error: invalid quantum '%s' (expected integer 1..1000)\n",
                        argv[2]);
                return 2;
            }
        }
        argi = 3;
    }
    if (argc > argi) {
        if (spec != NULL) {
            fprintf(stderr, "error: too many arguments for compare\n");
            return 2;
        }
        spec = argv[argi];
        argi++;
    }
    if (argc > argi) {
        fprintf(stderr, "error: too many arguments for compare\n");
        return 2;
    }

    if (load_jobs(spec, jobs, &n) != 0)
        return 2;

    return command_compare(jobs, n, quantum) == 0 ? 0 : 1;
}

static int command_cleanup(void)
{
    const char *patterns[] = {
        "/dev/shm/campuscompute_shm_*",
        "/dev/shm/sem.campuscompute_sem_*",
        "/dev/shm/campuscompute_sem_*",
    };
    int removed = 0;
    int found = 0;
    size_t p;
    int status;
    pid_t child;
    DIR *dir;
    struct dirent *entry;
    int threads = 0;
    int thread_scan_ok = 0;
    struct stat st;

    print_rule("Cleanup and resource verification");

    for (p = 0; p < sizeof(patterns) / sizeof(patterns[0]); p++) {
        glob_t matches;

        memset(&matches, 0, sizeof(matches));
        if (glob(patterns[p], 0, NULL, &matches) == 0) {
            size_t i;

            for (i = 0; i < matches.gl_pathc; i++) {
                found++;
                printf("Removing program IPC object: %s\n", matches.gl_pathv[i]);
                if (unlink(matches.gl_pathv[i]) != 0) {
                    fprintf(stderr, "warning: cannot unlink %s: %s\n",
                            matches.gl_pathv[i], strerror(errno));
                } else {
                    removed++;
                }
            }
        }
        globfree(&matches);
    }

    printf("Shared memory / named semaphore objects found: %d; removed: %d.\n",
           found, removed);
    if (found == 0)
        printf("No stale campuscompute IPC objects were present.\n");

    errno = 0;
    child = waitpid(-1, &status, WNOHANG);
    if (child < 0 && errno == ECHILD)
        printf("Child processes: none (all workers were reaped; no zombies).\n");
    else
        printf("Child processes: unexpected waitpid result (%s).\n",
               child < 0 ? strerror(errno) : "a child is still running");

    threads = 0;
    dir = opendir("/proc/self/task");
    if (dir != NULL) {
        thread_scan_ok = 1;
        while ((entry = readdir(dir)) != NULL) {
            if (entry->d_name[0] != '.')
                threads++;
        }
        closedir(dir);
    }
    printf("Live threads in this process: %d (main thread only).\n", threads);

    if (stat(LOG_FILE, &st) == 0)
        printf("Log file retained as evidence: %s (%lld bytes).\n", LOG_FILE,
               (long long)st.st_size);
    else
        printf("Log file not created yet: %s\n", LOG_FILE);

    printf("Cleanup verification complete.\n");
    return (removed == found && thread_scan_ok && threads == 1) ? 0 : 1;
}

static int command_demo(void)
{
    Job jobs[MAX_JOBS];
    int n;
    int ok_sched, ok_log, ok_ipc, ok_banker, ok_worker, ok_limit, ok_cleanup;
    int failed = 0;

    print_rule("CampusCompute integrated demonstration");
    printf("Students: Hrishikesh Das, Kailas Nath A\n");

    if (load_default_jobs(jobs, &n) != 0)
        return 1;

    print_rule("Demo 1/6: CPU scheduling comparison");
    ok_sched = command_compare(jobs, n, DEFAULT_QUANTUM) == 0;

    print_rule("Demo 2/6: concurrent logger");
    ok_log = command_logger(2) == 0;

    print_rule("Demo 3/6: shared-memory IPC");
    ok_ipc = command_ipc() == 0;

    print_rule("Demo 4/6: Banker's safety");
    ok_banker = command_banker("both") == 0;

    print_rule("Demo 5/6: worker lifecycle and limits");
    ok_worker = command_worker("both") == 0;
    ok_limit = command_limits() == 0;

    print_rule("Demo 6/6: cleanup");
    ok_cleanup = command_cleanup() == 0;

    if (!ok_sched || !ok_log || !ok_ipc || !ok_banker || !ok_worker ||
        !ok_limit || !ok_cleanup)
        failed = 1;

    printf("\n================ INTEGRATED SUMMARY ================\n");
    printf("Scheduling ....... %s (FCFS, RR q=%d, Priority)\n",
           ok_sched ? "PASS" : "FAIL", DEFAULT_QUANTUM);
    printf("Logger ........... %s (bounded queue, threads joined)\n",
           ok_log ? "PASS" : "FAIL");
    printf("IPC .............. %s (shared memory + named semaphore)\n",
           ok_ipc ? "PASS" : "FAIL");
    printf("Banker ........... %s (safe sequence; unsafe rejected)\n",
           ok_banker ? "PASS" : "FAIL");
    printf("Workers .......... %s (normal + simulated failure detected)\n",
           ok_worker ? "PASS" : "FAIL");
    printf("Resource limit ... %s (RLIMIT_CPU observed)\n",
           ok_limit ? "PASS" : "FAIL");
    printf("Cleanup .......... %s (no IPC objects remain)\n",
           ok_cleanup ? "PASS" : "FAIL");
    printf("Overall .......... %s\n", failed ? "FAIL" : "PASS");
    printf("Log file ......... %s\n", LOG_FILE);
    printf("====================================================\n");

    return failed;
}

static void menu_loop(void)
{
    Job jobs[MAX_JOBS];
    int n;

    if (load_default_jobs(jobs, &n) != 0)
        return;

    for (;;) {
        char line[64];
        int choice = -1;

        printf("\nCampusCompute menu\n");
        printf("  1. List jobs             2. FCFS schedule\n");
        printf("  3. Round Robin           4. Priority schedule\n");
        printf("  5. Compare policies      6. Threaded logger\n");
        printf("  7. Shared-memory IPC     8. Banker's safe state\n");
        printf("  9. Banker's unsafe state 10. Worker normal\n");
        printf(" 11. Worker failure        12. Resource limit\n");
        printf(" 13. Integrated demo      14. Cleanup check\n");
        printf("  0. Exit\n");
        printf("Choice: ");
        fflush(stdout);

        if (fgets(line, sizeof(line), stdin) == NULL)
            break;
        if (sscanf(line, "%d", &choice) != 1) {
            printf("Invalid choice: enter a number from the menu.\n");
            continue;
        }

        switch (choice) {
        case 0:
            printf("Goodbye.\n");
            return;
        case 1:
            print_rule("Built-in job set");
            print_job_table(jobs, n);
            printf("\nJob specification: %s\n", DEFAULT_JOB_SPEC);
            break;
        case 2: {
            ScheduleRun run;

            if (run_schedule(POLICY_FCFS, DEFAULT_QUANTUM, jobs, n, &run) == 0)
                print_schedule("FCFS", POLICY_FCFS, DEFAULT_QUANTUM, &run);
            break;
        }
        case 3: {
            ScheduleRun run;
            int q = DEFAULT_QUANTUM;

            printf("Quantum [%d]: ", DEFAULT_QUANTUM);
            fflush(stdout);
            if (fgets(line, sizeof(line), stdin) != NULL && line[0] != '\n') {
                if (parse_int_range(line, 1, 1000, &q) != 0) {
                    printf("Invalid quantum; using %d.\n", DEFAULT_QUANTUM);
                    q = DEFAULT_QUANTUM;
                }
            }
            if (run_schedule(POLICY_RR, q, jobs, n, &run) == 0)
                print_schedule("Round Robin", POLICY_RR, q, &run);
            break;
        }
        case 4: {
            ScheduleRun run;

            if (run_schedule(POLICY_PRIORITY, DEFAULT_QUANTUM, jobs, n, &run) == 0)
                print_schedule("Priority", POLICY_PRIORITY, DEFAULT_QUANTUM,
                               &run);
            break;
        }
        case 5:
            command_compare(jobs, n, DEFAULT_QUANTUM);
            break;
        case 6:
            command_logger(3);
            break;
        case 7:
            command_ipc();
            break;
        case 8:
            command_banker("safe");
            break;
        case 9:
            command_banker("unsafe");
            break;
        case 10:
            command_worker("normal");
            break;
        case 11:
            command_worker("fail");
            break;
        case 12:
            command_limits();
            break;
        case 13:
            command_demo();
            break;
        case 14:
            command_cleanup();
            break;
        default:
            printf("Invalid choice %d: choose 0..14.\n", choice);
            break;
        }
    }
}

int main(int argc, char **argv)
{
    const char *cmd;

    if (argc < 2) {
        menu_loop();
        return 0;
    }

    cmd = argv[1];

    if (strcmp(cmd, "help") == 0 || strcmp(cmd, "--help") == 0 ||
        strcmp(cmd, "-h") == 0) {
        print_usage(stdout);
        return 0;
    }
    if (strcmp(cmd, "menu") == 0) {
        menu_loop();
        return 0;
    }
    if (strcmp(cmd, "jobs") == 0)
        return command_jobs();
    if (strcmp(cmd, "schedule") == 0)
        return command_schedule(argc, argv);
    if (strcmp(cmd, "compare") == 0)
        return command_compare_cli(argc, argv);

    if (strcmp(cmd, "logger") == 0) {
        int events = 5;

        if (argc > 3) {
            fprintf(stderr, "error: logger accepts at most one argument\n");
            return 2;
        }
        if (argc == 3 && parse_int_range(argv[2], 1, 10, &events) != 0) {
            fprintf(stderr,
                    "error: events per producer must be an integer 1..10\n");
            return 2;
        }
        return command_logger(events);
    }

    if (strcmp(cmd, "ipc") == 0) {
        if (argc > 2) {
            fprintf(stderr, "error: ipc takes no arguments\n");
            return 2;
        }
        return command_ipc();
    }

    if (strcmp(cmd, "banker") == 0) {
        if (argc > 3) {
            fprintf(stderr, "error: banker accepts at most one argument\n");
            return 2;
        }
        return command_banker(argc == 3 ? argv[2] : "both");
    }

    if (strcmp(cmd, "worker") == 0) {
        if (argc > 3) {
            fprintf(stderr, "error: worker accepts at most one argument\n");
            return 2;
        }
        return command_worker(argc == 3 ? argv[2] : "both");
    }

    if (strcmp(cmd, "limits") == 0) {
        if (argc > 2) {
            fprintf(stderr, "error: limits takes no arguments\n");
            return 2;
        }
        return command_limits();
    }
    if (strcmp(cmd, "demo") == 0) {
        if (argc > 2) {
            fprintf(stderr, "error: demo takes no arguments\n");
            return 2;
        }
        return command_demo();
    }
    if (strcmp(cmd, "cleanup") == 0) {
        if (argc > 2) {
            fprintf(stderr, "error: cleanup takes no arguments\n");
            return 2;
        }
        return command_cleanup();
    }

    fprintf(stderr, "error: unknown command '%s'\n\n", cmd);
    print_usage(stderr);
    return 2;
}
