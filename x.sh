if [ ${2:-''} == 'unit' ]; then UNITS_PREFIX="units"; else UNITS_PREFIX="."; fi

CC=gcc
CFLAGS='-O1 -Wall -Werror -march=native'
SHARED_SRC='util.c shared.c'
SENDER_SRC="$UNITS_PREFIX/sender.c"
RECEIVER_SRC="$UNITS_PREFIX/receiver.c"

if [ $1 == 'sender' ]; then $CC $CFLAGS $SENDER_SRC $SHARED_SRC -o sender && taskset -c 0 ./sender;
else $CC $CFLAGS $RECEIVER_SRC $SHARED_SRC -o receiver && taskset -c 4 ./receiver; fi