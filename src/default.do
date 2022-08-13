#!/bin/execlineb -S3
multisubstitute {
	importas -D "cc" CC CC
	importas -sD "" LDFLAGS LDFLAGS
}
foreground { redo-ifchange ${2}.c.o }
$CC $LDFLAGS -o $3 ${2}.c.o -ltertium
