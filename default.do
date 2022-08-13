#!/bin/execlineb -S3
backtick PROGS { pipeline { find src -type f -name "*.c" } sed "s;.c$;;" }
multisubstitute {
	importas -D "/usr/local" DESTDIR DESTDIR
	importas -D "/bin" BINDIR BINDIR
	importas -D "/share/man" MANDIR MANDIR
	importas -isu PROGS PROGS
	elglob MANPAGES "man/*"
}
ifelse { test "${1}" = "all" } {
	redo-ifchange $PROGS
}
ifelse { test "${1}" = "clean" } {
	backtick targets { redo-targets }
	importas -isu targets targets
	rm -f $targets
}
ifelse { test "${1}" = "install" } {
	foregroud { redo-ifchange all }
	foreground { install -dm "${DESTDIR}/${BINDIR}" }
	foreground { install -dm "${DESTDIR}/${MANDIR}/man1" }
	foreground { install -cm 755 $PROGS "${DESTDIR}/${BINDIR}" }
	install -cm 644 $MANPAGES "${DESTDIR}/${MANDIR}/man1"
}
exit 0
