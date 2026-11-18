#!/usr/bin/env bash
# FR8 - Command-line interface and error handling
set -u
cd "$(dirname "$0")/.." || exit 1
if [[ ! -x ./campuscompute ]]; then
    echo "Building campuscompute..."
    make || exit 1
fi

echo "============================================================"
echo "FR8 - Command-line interface and error handling"
echo "============================================================"
echo "Every feature is independently testable from the CLI; malformed input"
echo "must produce a useful message and a non-zero exit status."
echo

./campuscompute help
echo
echo "Invalid scheduling policy:"
./campuscompute schedule bogus
echo "exit status: $?"
echo
echo "Invalid Round Robin quantum:"
./campuscompute schedule rr 0
echo "exit status: $?"
echo
echo "Malformed job specification:"
./campuscompute schedule fcfs '1:0:8'
echo "exit status: $?"
echo
echo "Unknown command:"
./campuscompute frobnicate 2>&1 | head -n 4
echo "exit status: ${PIPESTATUS[0]}"
exit 0
