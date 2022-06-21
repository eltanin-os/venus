#!/bin/rc -e
if (~ $1 *.c) exit
redo-ifchange $2.c.o $LIBS
$CC $LDFLAGS -o $3 $2.c.o $LIBS -ltertium
