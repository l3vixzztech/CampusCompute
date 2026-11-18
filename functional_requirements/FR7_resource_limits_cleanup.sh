#!/usr/bin/env bash
# FR7 - Process resource limit and cleanup verification
set -u
cd "$(dirname "$0")/.." || exit 1
if [[ ! -x ./campuscompute ]]; then
    echo "Building campuscompute..."
    make || exit 1
fi

echo "============================================================"
echo "FR7 - Resource limits and cleanup"
echo "============================================================"
echo "A child sets RLIMIT_CPU to a 1-second soft limit and 2-second hard limit,"
echo "then runs a CPU-bound loop. The parent observes SIGXCPU."
echo "Cleanup then verifies children, threads and IPC objects are gone."
echo

./campuscompute limits
echo
./campuscompute cleanup
exit 0
