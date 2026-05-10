set -euo pipefail

CC="gcc"
CFLAGS="-O2 -march=native"
DELAY=300000000 # 300M cycles; ~135ms

$CC $CFLAGS send.c -o send
$CC $CFLAGS recv.c -o recv
$CC $CFLAGS tsc.c -o tsc

read -r -p "> " MSG

NOW=$(taskset -c 3 ./tsc)
START=$(( (NOW + DELAY + 2047) & ~2047 ))

taskset -c 3 ./recv $START &
printf '%s\n' "$MSG" | taskset -c 7 ./send $START &
wait
