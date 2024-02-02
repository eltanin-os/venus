#!/bin/execlineb -P
backtick packages { redo-ood . }
importas -isu packages packages
forx -E pkg { $packages }
if -nt { test -e "${pkg}.do" } # rule?
if -nt { test -e "${pkg}" } # orphan?
if {
	backtick files {
		pipeline { redo-targets $pkg }
		grep "/root/"
	}
	importas -isu files files
	rm -f $files
}
foreground {
	backtick directories {
		pipeline { redo-targets $pkg }
		awk
"
/\\/root\\// {
	sub(/\\/[A-Za-z0-9_-]*$/, \"\", path)
	dict[path] = path
}
END {
	for (key in dict) print dict[key]
}
"
	}
	redirfd -w 2 /dev/null
	importas -isu directories directories
	rmdir $directories
}
# XXX: redo should track the filesystem state
if { rm ../.redo/${pkg}.dep }
# a dep removed implies the death of its exclusive metadeps
elglob metadeps "../.redo/${pkg}#*.dep"
rm -f $metadeps
