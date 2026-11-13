#include "worker.h"

#include "common.h"

#include <signal.h>
#include <sys/wait.h>

static int spawn_and_wait(int index, int simulate_failure)
{
    pid_t pid;
    int status;
    pid_t waited;

    fflush(NULL);
    pid = fork();
    if (pid < 0) {
        fprintf(stderr, "error: fork failed: %s\n", strerror(errno));
        return 1;
    }

    if (pid == 0) {
        printf("  worker %d (pid %d): started\n", index, (int)getpid());
        fflush(NULL);

        if (simulate_failure == 1) {
            printf("  worker %d (pid %d): SIMULATED failure - exiting with code 42\n",
                   index, (int)getpid());
            fflush(NULL);
            _exit(42);
        }
        if (simulate_failure == 2) {
            printf("  worker %d (pid %d): SIMULATED failure - raising SIGSEGV\n",
                   index, (int)getpid());
            fflush(NULL);
            raise(SIGSEGV);
            _exit(43);
        }

        sleep(1);
        printf("  worker %d (pid %d): normal work complete, exiting 0\n", index,
               (int)getpid());
        fflush(NULL);
        _exit(0);
    }

    do {
        waited = waitpid(pid, &status, 0);
    } while (waited < 0 && errno == EINTR);

    if (waited < 0) {
        fprintf(stderr, "error: waitpid(%d): %s\n", (int)pid, strerror(errno));
        return 1;
    }

    if (WIFEXITED(status)) {
        int code = WEXITSTATUS(status);

        if (code == 0) {
            printf("parent: worker %d (pid %d) exited normally, status 0.\n",
                   index, (int)pid);
            if (simulate_failure) {
                fprintf(stderr,
                        "error: simulated failure did not occur for worker %d\n",
                        index);
                return 1;
            }
            return 0;
        }
        printf("parent: worker %d (pid %d) exited with non-zero status %d%s.\n",
               index, (int)pid, code,
               simulate_failure ? " (simulated failure detected)" : "");
        return simulate_failure ? 0 : 1;
    }

    if (WIFSIGNALED(status)) {
        int sig = WTERMSIG(status);

        printf("parent: worker %d (pid %d) terminated by signal %d (%s)%s.\n",
               index, (int)pid, sig, strsignal(sig),
               simulate_failure ? " (simulated failure detected)" : "");
        return simulate_failure ? 0 : 1;
    }

    printf("parent: worker %d (pid %d) ended with unrecognised status.\n",
           index, (int)pid);
    return 1;
}

static int check_no_children(void)
{
    int status;
    pid_t p;

    errno = 0;
    p = waitpid(-1, &status, WNOHANG);
    if (p < 0 && errno == ECHILD) {
        printf("parent: no child processes remain (all reaped, no zombies).\n");
        return 0;
    }
    if (p == 0) {
        printf("parent: WARNING - a child process is still running.\n");
        return 1;
    }
    printf("parent: WARNING - unexpected child state.\n");
    return 1;
}

int command_worker(const char *mode)
{
    int failures = 0;

    if (mode == NULL || strcmp(mode, "both") == 0) {
        print_rule("Worker lifecycle: normal completion");
        failures += spawn_and_wait(1, 0);
        failures += spawn_and_wait(2, 0);
        failures += check_no_children();

        print_rule("Worker lifecycle: controlled simulated failure");
        printf("Note: these are deliberate, simulated failures - not real OS faults.\n");
        failures += spawn_and_wait(3, 1);
        failures += spawn_and_wait(4, 2);
        failures += spawn_and_wait(5, 0);
        failures += check_no_children();
    } else if (strcmp(mode, "normal") == 0) {
        print_rule("Worker lifecycle: normal completion");
        failures += spawn_and_wait(1, 0);
        failures += spawn_and_wait(2, 0);
        failures += spawn_and_wait(3, 0);
        failures += check_no_children();
    } else if (strcmp(mode, "fail") == 0) {
        print_rule("Worker lifecycle: controlled simulated failure");
        printf("Note: these are deliberate, simulated failures - not real OS faults.\n");
        failures += spawn_and_wait(1, 1);
        failures += spawn_and_wait(2, 2);
        failures += check_no_children();
    } else {
        fprintf(stderr, "error: worker mode must be normal, fail, or both\n");
        return 2;
    }

    return failures == 0 ? 0 : 1;
}
