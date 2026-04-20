CC=gcc
CFLAGS='-O0 -Wall -Werror'
SHARED_SRC='util.c shared.c'
SENDER_SRC='sender.c'
RECEIVER_SRC='receiver.c'

if [ $1 == 'sender' ]; then $CC $CFLAGS $SENDER_SRC $SHARED_SRC -o sender && taskset -c 0 ./sender;
else $CC $CFLAGS $RECEIVER_SRC $SHARED_SRC -o receiver && taskset -c 4 ./receiver; fi