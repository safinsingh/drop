CC="gcc"
CFLAGS="-O2 -march=native"
SHARED_SRC="shared.c"
CORE=3

if [ $1 == "send" ]; then CORE=$((CORE+4)); fi

$CC $CFLAGS $SHARED_SRC "$1.c" -o $1 && taskset -c $CORE ./$1
