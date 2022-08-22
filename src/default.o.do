#!/bin/execlineb -S3
multisubstitute {
	importas -sD "cc" CC CC
	importas -sD "-O0 -g -std=c99 -Wall -Wextra -pedantic" CFLAGS CFLAGS
	importas -sD "" CPPFLAGS CPPFLAGS
}
foreground { redo-ifchange $2 }
$CC $CFLAGS $CPPFLAGS -o $3 -c $2
