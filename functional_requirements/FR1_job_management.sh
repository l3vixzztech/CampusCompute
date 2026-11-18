#!/usr/bin/env bash
# FR1 - Job management and process lifecycle
set -u
cd "$(dirname "$0")/.." || exit 1
if [[ ! -x ./campuscompute ]]; then
    echo "Building campuscompute..."
    make || exit 1
fi

echo "============================================================"
echo "FR1 - Job management and process lifecycle"
echo "============================================================"
echo "Each job has an ID, arrival time, CPU burst and priority."
echo "Scheduling records its start, completion, turnaround and waiting time."
echo

./campuscompute jobs
echo
echo "Custom 3-job set, scheduled with FCFS (lifecycle: arrival -> start -> completion):"
./campuscompute schedule fcfs '10:0:6:2,11:2:3:1,12:4:5:3'
echo
echo "Validation check: a duplicate job ID must be rejected."
./campuscompute schedule fcfs '1:0:8:2,1:1:4:1' || true
exit 0
