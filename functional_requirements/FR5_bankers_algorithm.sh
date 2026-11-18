#!/usr/bin/env bash
# FR5 - Banker's resource-safety algorithm
set -u
cd "$(dirname "$0")/.." || exit 1
if [[ ! -x ./campuscompute ]]; then
    echo "Building campuscompute..."
    make || exit 1
fi

echo "============================================================"
echo "FR5 - Banker's safety algorithm"
echo "============================================================"
echo "Need = Maximum - Allocation. The safety check compares Need with Work"
echo "and adds Allocation back to Work after each process can finish."
echo

./campuscompute banker both
exit 0
