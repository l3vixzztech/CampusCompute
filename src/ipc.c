#include "ipc.h"

#include "common.h"

#include <fcntl.h>
#include <semaphore.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>

#define IPC_MAGIC 0xC0FFEEL

typedef struct {
    long magic;
    pid_t writer_pid;
    int job_id;
    long input;
    long result;
    char message[64];
} IpcPayload;

static int append_name(char *dst, size_t len, const char *prefix, long pid)
{
    int n = snprintf(dst, len, "/%s_%ld", prefix, pid);

    return (n > 0 && (size_t)n < len) ? 0 : -1;
}

int command_ipc(void)
{
    char shm_name[64];
    char sem_name[64];
    int shm_fd = -1;
    sem_t *sem = SEM_FAILED;
    IpcPayload *shared = MAP_FAILED;
    pid_t child = -1;
    int sem_created = 0;
    int shm_created = 0;
    int rc = 0;
    int status = 0;

    if (append_name(shm_name, sizeof(shm_name), "campuscompute_shm",
                    (long)getpid()) != 0 ||
        append_name(sem_name, sizeof(sem_name), "campuscompute_sem",
                    (long)getpid()) != 0) {
        fprintf(stderr, "error: IPC name is too long\n");
        return 1;
    }

    print_rule("POSIX shared memory and named semaphore IPC");
    printf("Shared memory object: %s\n", shm_name);
    printf("Named semaphore:      %s\n", sem_name);

    shm_fd = shm_open(shm_name, O_CREAT | O_EXCL | O_RDWR, 0600);
    if (shm_fd < 0) {
        fprintf(stderr, "error: shm_open(%s): %s\n", shm_name, strerror(errno));
        return 1;
    }
    shm_created = 1;

    if (ftruncate(shm_fd, (off_t)sizeof(IpcPayload)) != 0) {
        fprintf(stderr, "error: ftruncate: %s\n", strerror(errno));
        rc = 1;
        goto cleanup;
    }

    shared = mmap(NULL, sizeof(IpcPayload), PROT_READ | PROT_WRITE,
                  MAP_SHARED, shm_fd, 0);
    if (shared == MAP_FAILED) {
        fprintf(stderr, "error: mmap: %s\n", strerror(errno));
        rc = 1;
        goto cleanup;
    }
    memset(shared, 0, sizeof(*shared));

    sem = sem_open(sem_name, O_CREAT | O_EXCL, 0600, 0);
    if (sem == SEM_FAILED) {
        fprintf(stderr, "error: sem_open(%s): %s\n", sem_name,
                strerror(errno));
        rc = 1;
        goto cleanup;
    }
    sem_created = 1;
    printf("Parent: buffers created and mapped; semaphore initial value = 0.\n");

    fflush(NULL);
    child = fork();
    if (child < 0) {
        fprintf(stderr, "error: fork: %s\n", strerror(errno));
        rc = 1;
        goto cleanup;
    }

    if (child == 0) {
        /* Child worker: write a computed result to the shared mapping. */
        int err;

        shared->magic = IPC_MAGIC;
        shared->writer_pid = getpid();
        shared->job_id = 7;
        shared->input = 12345;
        shared->result = shared->input * 2 + 7;
        snprintf(shared->message, sizeof(shared->message),
                 "result computed in child process");

        printf("Child  (pid %d): wrote job %d input=%ld result=%ld into %s\n",
               (int)getpid(), shared->job_id, shared->input, shared->result,
               shm_name);
        fflush(NULL);
        err = sem_post(sem);
        if (err != 0) {
            fprintf(stderr, "child: sem_post failed: %s\n", strerror(errno));
            _exit(2);
        }
        printf("Child  (pid %d): sem_post() signalled the parent; exiting 0\n",
               (int)getpid());
        fflush(NULL);
        _exit(0);
    }

    printf("Parent (pid %d): forked child worker pid %d\n", (int)getpid(),
           (int)child);

    {
        struct timespec deadline;
        int wait_rc;

        clock_gettime(CLOCK_REALTIME, &deadline);
        deadline.tv_sec += 5;
        do {
            wait_rc = sem_timedwait(sem, &deadline);
        } while (wait_rc != 0 && errno == EINTR);

        if (wait_rc != 0) {
            fprintf(stderr, "error: timed out waiting for child semaphore: %s\n",
                    strerror(errno));
            kill(child, SIGKILL);
            rc = 1;
            goto reap;
        }
    }

    printf("Parent: sem_wait()/sem_timedwait() returned after child signalled.\n");
    if (shared->magic != IPC_MAGIC || shared->writer_pid != child) {
        fprintf(stderr, "error: shared memory payload validation failed\n");
        rc = 1;
        goto reap;
    }

    printf("Parent: read job=%d input=%ld result=%ld from shared memory.\n",
           shared->job_id, shared->input, shared->result);
    printf("Parent: message=\"%s\" written by pid %d.\n", shared->message,
           (int)shared->writer_pid);
    printf("IPC exchange verified: child wrote, semaphore synchronised, parent read.\n");

reap:
    {
        pid_t waited;

        do {
            waited = waitpid(child, &status, 0);
        } while (waited < 0 && errno == EINTR);

        if (waited < 0) {
            fprintf(stderr, "error: waitpid(%d): %s\n", (int)child,
                    strerror(errno));
            rc = 1;
        } else if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
            printf("Parent: child %d exited normally with status 0.\n",
                   (int)child);
        } else if (WIFEXITED(status)) {
            fprintf(stderr, "error: child exited with status %d\n",
                    WEXITSTATUS(status));
            rc = 1;
        } else if (WIFSIGNALED(status)) {
            fprintf(stderr, "error: child terminated by signal %d (%s)\n",
                    WTERMSIG(status), strsignal(WTERMSIG(status)));
            rc = 1;
        }
    }

cleanup:
    if (shared != MAP_FAILED)
        munmap(shared, sizeof(IpcPayload));
    if (shm_fd >= 0)
        close(shm_fd);
    if (sem_created) {
        sem_close(sem);
        if (sem_unlink(sem_name) != 0) {
            fprintf(stderr, "warning: sem_unlink(%s): %s\n", sem_name,
                    strerror(errno));
            rc = 1;
        } else {
            printf("Cleanup: sem_unlink(%s) removed the named semaphore.\n",
                   sem_name);
        }
    }
    if (shm_created) {
        if (shm_unlink(shm_name) != 0) {
            fprintf(stderr, "warning: shm_unlink(%s): %s\n", shm_name,
                    strerror(errno));
            rc = 1;
        } else {
            printf("Cleanup: shm_unlink(%s) removed the shared memory object.\n",
                   shm_name);
        }
    }

    if (shm_created) {
        int fd = shm_open(shm_name, O_RDONLY, 0);

        if (fd >= 0) {
            fprintf(stderr, "error: shared memory object still exists after unlink\n");
            close(fd);
            rc = 1;
        } else if (errno == ENOENT) {
            printf("Verification: %s no longer exists.\n", shm_name);
        }
    }
    if (sem_created) {
        sem_t *check = sem_open(sem_name, 0);

        if (check != SEM_FAILED) {
            fprintf(stderr, "error: named semaphore still exists after unlink\n");
            sem_close(check);
            rc = 1;
        } else if (errno == ENOENT) {
            printf("Verification: semaphore %s no longer exists.\n", sem_name);
        }
    }

    return rc;
}
