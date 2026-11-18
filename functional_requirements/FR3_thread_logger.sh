#!/usr/bin/env bash
# FR3 - Concurrent activity logger with POSIX threads
set -u
cd "$(dirname "$0")/.." || exit 1
if [[ ! -x ./campuscompute ]]; then
    echo "Building campuscompute..."
    make || exit 1
fi

echo "============================================================"
echo "FR3 - Concurrent activity logger"
echo "============================================================"
echo "3 producer threads and 1 consumer share a bounded queue (capacity 8)."
echo "A mutex protects the queue; not_empty/not_full condition variables block"
echo "producers when the queue is full and the consumer when it is empty."
echo "Watch the 'queue full' lines, then the clean joins at the end."
echo

./campuscompute logger 5
echo
echo "Generated log entries:"
tail -n 8 logs/campuscompute.log
exit 0
