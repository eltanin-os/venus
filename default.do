#!/bin/rc -e
if (~ $1 *.[1ch]) exit
MAINDIR=$PWD
if (test -e config.rc) {
	redo-ifchange config.rc
	. $MAINDIR/config.rc ||;
}; if not {
	redo-ifcreate config.rc
}
PROGS=`{find src -type f -name '*.c' | sed 's;.c$;;'}
MANPAGES=man/*
switch ($1) {
case all
	redo-ifchange $PROGS
case clean
	rm -f `{redo-targets}
case install
	redo-always
	redo-ifchange all install-man
	install -dm 755 $"DESTDIR/$"BINDIR
	install -cm 755 $PROGS $"DESTDIR/$"BINDIR
case install-man
	redo-always
	redo-ifchange $MANPAGES
	install -dm 755 $"DESTDIR/$"MANDIR/man1
	install -cm 644 $MANPAGES $"DESTDIR/$"MANDIR/man1
case *
	echo no rule for ''''$1'''' >[1=2]
	exit 1
}
