#!/usr/bin/env bash
# FR4 - POSIX shared memory and named semaphore IPC
set -u
cd "$(dirname "$0")/.." || exit 1
if [[ ! -x ./campuscompute ]]; then
    echo "Building campuscompute..."
    make || exit 1
fi

echo "============================================================"
echo "FR4 - Inter-process communication (shared memory + semaphore)"
echo "============================================================"
echo "The parent creates a uniquely named shared-memory object and semaphore,"
echo "forks a child, and waits for the child to write a result and sem_post()."
echo

./campuscompute ipc
echo
echo "Post-run check: no IPC objects should remain."
ls /dev/shm | grep campuscompute || echo "no campuscompute IPC objects remain"
exit 0
