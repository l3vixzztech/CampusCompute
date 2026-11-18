# Functional Requirement Demos

Run the project's eight functional requirements one at a time. Each script
builds `campuscompute` automatically if the binary is missing, then runs the
real implementation (no mock output).

```sh
cd ~/Desktop/OS_Project

./functional_requirements/FR1_job_management.sh
./functional_requirements/FR2_cpu_scheduling.sh
./functional_requirements/FR3_thread_logger.sh
./functional_requirements/FR4_shared_memory_ipc.sh
./functional_requirements/FR5_bankers_algorithm.sh
./functional_requirements/FR6_worker_failure.sh
./functional_requirements/FR7_resource_limits_cleanup.sh
./functional_requirements/FR8_cli_error_handling.sh
```

| FR | Script | What it demonstrates |
|----|--------|----------------------|
| FR1 | `FR1_job_management.sh` | Job fields (ID, arrival, burst, priority), validated custom jobs, start/completion lifecycle |
| FR2 | `FR2_cpu_scheduling.sh` | FCFS, Round Robin (quantum 4), non-preemptive Priority, and their comparison |
| FR3 | `FR3_thread_logger.sh` | 3 producers + 1 consumer over a bounded queue with mutex/condition variables, queue-full waits, clean joins, log file |
| FR4 | `FR4_shared_memory_ipc.sh` | Parent/child data exchange through POSIX shared memory, synchronised by a named semaphore, then unlinked |
| FR5 | `FR5_bankers_algorithm.sh` | `Need = Maximum - Allocation`, `Work`/`Finish` safety check, safe sequence and unsafe rejection |
| FR6 | `FR6_worker_failure.sh` | `fork()`/`waitpid()`, normal exit 0, simulated exit 42 and SIGSEGV, no zombies |
| FR7 | `FR7_resource_limits_cleanup.sh` | `RLIMIT_CPU` on a child (`SIGXCPU`), then cleanup of children, threads and IPC objects |
| FR8 | `FR8_cli_error_handling.sh` | Help text, invalid policy/quantum/job spec/command, useful errors and non-zero exit codes |

## Short explanation points

- **FR1:** Jobs are validated before scheduling; the table shows arrival, burst and priority, and the schedule shows start, completion, TAT and waiting time.
- **FR2:** FCFS and Priority are non-preemptive; Round Robin uses quantum 4. Tie-breaks: arrival then job ID; Priority uses lower number first; RR uses a FIFO ready queue.
- **FR3:** The queue holds at most 8 events. Producers wait on `not_full`, the consumer waits on `not_empty`, and all four threads are joined before the mutex/condition variables are destroyed.
- **FR4:** The child writes the result into shared memory and calls `sem_post`; the parent waits with a timeout, validates the payload, reaps the child, and unlinks the shared memory and semaphore.
- **FR5:** The safe case prints a complete safe sequence; the unsafe case shows where no remaining process satisfies `Need <= Work`.
- **FR6:** Failures are explicitly simulated. The parent distinguishes normal exit, non-zero exit and signal termination.
- **FR7:** The CPU limit is applied only to the child; the parent reports the observed signal. Cleanup confirms no child processes, extra threads or project IPC objects remain.
- **FR8:** Errors go to stderr and return non-zero, so each feature can be tested independently.
