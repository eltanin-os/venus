#!/bin/execlineb -S3
backtick PROGS { pipeline { find src -type f -name "*.c" } sed "s;.c$;;" }
multisubstitute {
	importas -D "" DESTDIR DESTDIR
	importas -D "/usr/local" PREFIX PREFIX
	importas -D "/bin" BINDIR BINDIR
	importas -D "/share/man" MANDIR MANDIR
	importas -isu PROGS PROGS
	elglob MANPAGES "man/*"
}
case -- $1 {
	".*\.[1ch]" {
		exit 0
	}
	"all" {
		redo-ifchange $PROGS
	}
	"clean" {
		backtick targets { redo-targets }
		importas -isu targets targets
		rm -f $targets
	}
	"install" {
		if { redo-ifchange all }
		if { install -dm 755 "${DESTDIR}${PREFIX}${BINDIR}" }
		if { install -dm 755 "${DESTDIR}${PREFIX}${MANDIR}/man1" }
		if { install -cm 755 $PROGS "${DESTDIR}${PREFIX}/${BINDIR}" }
		install -cm 644 $MANPAGES "${DESTDIR}${PREFIX}${MANDIR}/man1"
	}
	"install-store" {
		cp -R pm "${DESTDIR}${PREFIX}/venus-store"
	}
}
foreground {
	fdmove 1 2
	echo no rule for $1
}
exit 1
