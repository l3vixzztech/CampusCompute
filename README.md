# CampusCompute

CampusCompute is a command-line prototype for a university shared-computing lab.
It demonstrates job scheduling, a synchronized activity logger, POSIX shared-memory
IPC, Banker's resource-safety analysis, worker failure detection, and process
resource limits.

Students:

- Hrishikesh Das (PES1UG25CS209)
- Kailas Nath A (PES1UG25CS238)

## Prerequisites

- Linux or WSL Ubuntu
- GCC, GNU Make, and the POSIX threads/runtime libraries
- A terminal with at least 120 columns for the wider scheduling tables

Check the toolchain:

```sh
gcc --version
make --version
```

## Build

```sh
make
```

This produces the `./campuscompute` executable. To rebuild from scratch:

```sh
make clean
make
```

## Run

Show the command reference:

```sh
./campuscompute help
```

Run the integrated demonstration, which exercises every subsystem and prints a
PASS/FAIL summary:

```sh
./campuscompute demo
```

Individual features are independently testable:

```sh
./campuscompute jobs
./campuscompute schedule fcfs
./campuscompute schedule rr 4
./campuscompute schedule priority
./campuscompute compare
./campuscompute logger 4
./campuscompute ipc
./campuscompute banker safe
./campuscompute banker unsafe
./campuscompute worker normal
./campuscompute worker fail
./campuscompute limits
./campuscompute cleanup
```

Run `./campuscompute` with no arguments for the interactive menu.

### Custom job sets

Scheduling commands accept an optional job specification:

```sh
./campuscompute schedule rr 2 '1:0:8:2,2:1:4:1,3:2:9:4'
```

Format: `id:arrival:burst:priority`, comma separated. IDs must be unique.
Validation ranges: ID 1..9999, arrival 0..10000, burst 1..100, priority 1..99.

### Scheduling rules

- **FCFS:** non-preemptive; earlier arrival first, then smaller job ID.
- **Round Robin:** default quantum 4; FIFO ready queue; simultaneous arrivals are
  queued by job ID; jobs that arrive during a slice join before the preempted job.
- **Priority:** non-preemptive; lower priority number means higher priority, then
  earlier arrival, then smaller job ID.
- Every policy reports start/completion times, turnaround time, waiting time, and
  averages for the same job set.

## Design notes

- **Logger (FR3):** three producer threads and one consumer share a bounded queue
  (capacity 8). A mutex protects the queue; `not_empty` and `not_full` condition
  variables coordinate blocking. Producers wait when the queue is full, the
  consumer waits when it is empty, and every thread is joined before the mutex and
  condition variables are destroyed. Events are timestamped in
  `logs/campuscompute.log`.
- **IPC (FR4):** the parent creates a uniquely named POSIX shared-memory object and
  named semaphore (`/campuscompute_*_<pid>`). A forked child writes a computed
  result into shared memory and posts the semaphore; the parent waits with a
  five-second `sem_timedwait()` deadline, reads and validates the payload, reaps
  the child, then unlinks both IPC objects.
- **Banker's algorithm (FR5):** computes `Need = Maximum - Allocation`, runs the
  safety algorithm with `Work` and `Finish`, prints a complete safe sequence for
  the safe case, and reports the unsafe case where no remaining process fits.
- **Workers (FR6):** `fork()` creates child workers and `waitpid()` collects their
  status. The failure mode is explicitly simulated (exit status 42 and a raised
  `SIGSEGV`); it is not a real OS fault.
- **Resource limit (FR7):** the `limits` command forks a child, applies
  `RLIMIT_CPU` of one second to the child only, and reports the kernel signal that
  terminates the CPU-bound loop.
- **Cleanup (FR7):** IPC objects are unlinked on every path. `./campuscompute
  cleanup` removes any stale objects with the project's unique
  `campuscompute_` prefix, checks for unreaped children and extra threads, and
  reports the log file. `make clean` removes objects, the executable, and the log.

## Logs

The logger writes `logs/campuscompute.log` with millisecond timestamps. The log is
retained as evidence; `make clean` deletes it.

## Cleanup

```sh
./campuscompute cleanup
make clean
```

`cleanup` only removes shared-memory and semaphore names created under the
project's `campuscompute_` prefix. It does not touch unrelated system IPC objects.
