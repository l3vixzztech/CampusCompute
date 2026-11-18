#!/usr/bin/env bash
# FR2 - FCFS, Round Robin and non-preemptive Priority scheduling
set -u
cd "$(dirname "$0")/.." || exit 1
if [[ ! -x ./campuscompute ]]; then
    echo "Building campuscompute..."
    make || exit 1
fi

echo "============================================================"
echo "FR2 - CPU scheduling: FCFS, Round Robin, Priority"
echo "============================================================"
echo "FCFS tie-break: earlier arrival, then smaller job ID."
echo "Round Robin quantum = 4, FIFO ready queue, simultaneous arrivals by ID."
echo "Priority: lower number = higher priority, then arrival, then job ID."
echo

./campuscompute schedule fcfs
echo
./campuscompute schedule rr 4
echo
./campuscompute schedule priority
echo
./campuscompute compare
exit 0
