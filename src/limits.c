#include "limits.h"

#include "common.h"

#include <signal.h>
#include <sys/resource.h>
#include <sys/wait.h>

static void print_limit_value(const char *label, rlim_t value)
{
    if (value == RLIM_INFINITY)
        printf("%s=unlimited", label);
    else
        printf("%s=%llu s", label, (unsigned long long)value);
}

int command_limits(void)
{
    struct rlimit inherited;
    pid_t pid;
    int status;
    pid_t waited;

    print_rule("Process resource limit (setrlimit / RLIMIT_CPU)");

    if (getrlimit(RLIMIT_CPU, &inherited) != 0) {
        fprintf(stderr, "error: getrlimit(RLIMIT_CPU): %s\n", strerror(errno));
        return 1;
    }

    printf("Inherited RLIMIT_CPU: ");
    print_limit_value("soft", inherited.rlim_cur);
    printf(", ");
    print_limit_value("hard", inherited.rlim_max);
    printf("\n");

    fflush(NULL);
    pid = fork();
    if (pid < 0) {
        fprintf(stderr, "error: fork: %s\n", strerror(errno));
        return 1;
    }

    if (pid == 0) {
        struct rlimit limit;

        limit.rlim_cur = 1;
        limit.rlim_max = 2;
        if (setrlimit(RLIMIT_CPU, &limit) != 0) {
            fprintf(stderr, "child: setrlimit(RLIMIT_CPU): %s\n",
                    strerror(errno));
            _exit(3);
        }

        if (getrlimit(RLIMIT_CPU, &limit) != 0) {
            fprintf(stderr, "child: getrlimit(RLIMIT_CPU): %s\n",
                    strerror(errno));
            _exit(4);
        }

        printf("child (pid %d): applied RLIMIT_CPU ", (int)getpid());
        print_limit_value("soft", limit.rlim_cur);
        printf(", ");
        print_limit_value("hard", limit.rlim_max);
        printf("\n");
        printf("child: entering CPU-bound loop; the kernel should deliver SIGXCPU after 1 CPU second.\n");
        printf("child: hard limit is 2 s, so SIGXCPU is not immediately replaced by SIGKILL.\n");
        fflush(NULL);

        {
            volatile unsigned long long spin = 0;

            for (;;)
                spin++;
        }
    }

    printf("parent: forked child %d with a 1-second CPU soft limit and 2-second hard limit.\n",
           (int)pid);
    printf("parent: waiting for the child to hit the limit...\n");

    do {
        waited = waitpid(pid, &status, 0);
    } while (waited < 0 && errno == EINTR);

    if (waited < 0) {
        fprintf(stderr, "error: waitpid(%d): %s\n", (int)pid, strerror(errno));
        return 1;
    }

    if (WIFSIGNALED(status)) {
        int sig = WTERMSIG(status);

        printf("parent: child terminated by signal %d (%s).\n", sig,
               strsignal(sig));
        if (sig == SIGXCPU || sig == SIGKILL) {
            printf("Observed effect: the kernel stopped the CPU-bound child after it exceeded the 1-second CPU soft limit.\n");
            printf("The limit was applied only to the child; the parent process is unaffected.\n");
            return 0;
        }
        printf("Unexpected signal; the resource-limit demonstration did not match SIGXCPU.\n");
        return 1;
    }

    if (WIFEXITED(status)) {
        fprintf(stderr,
                "error: child exited with status %d instead of hitting the CPU limit\n",
                WEXITSTATUS(status));
        return 1;
    }

    fprintf(stderr, "error: child ended in an unexpected state\n");
    return 1;
}
