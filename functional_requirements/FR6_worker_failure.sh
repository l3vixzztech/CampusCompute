#!/usr/bin/env bash
# FR6 - Worker processes and failure detection
set -u
cd "$(dirname "$0")/.." || exit 1
if [[ ! -x ./campuscompute ]]; then
    echo "Building campuscompute..."
    make || exit 1
fi

echo "============================================================"
echo "FR6 - Worker failure detection with fork() and waitpid()"
echo "============================================================"
echo "Normal workers exit 0. The failure cases are deliberately simulated:"
echo "one exits with status 42, the other raises SIGSEGV."
echo "The parent reports WIFEXITED/WIFSIGNALED and reaps every child."
echo

./campuscompute worker both
exit 0
