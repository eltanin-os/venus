#!/bin/execlineb -S3
backtick -Ex progname { pipeline { echo $1 } cut -d"#" -f1 }
case -- $1 {
	"all" { exit 0 }
	# TODO: version check "name" = "name#latest"
	".*#.*" { redo-ifchange $progname }
}
if {
	backtick -Ex datafile { ../repo/get $progname }
	redo-ifchange $datafile
}
backtick -Ex name { ../repo/get -k name $progname }
ifelse {
	case -- $progname {
		"${name}|${name}-dev|${name}-dyn" { false }
	}
	true
} {
	backtick -Ex dep {
		case -- $progname {
			".*-dev" { echo "${name}-dev" }
			".*-dyn" { echo "${name}-dyn" }
		}
		echo "${name}"
	}
	if { redo-ifchange $dep }
	ln -s $dep $3
}
if {
	if {
		if -t { ../repo/get ${progname}-dyn }
		redo-ifchange ${progname}-dyn
	}
	backtick rundeps { ../repo/get -t rundeps $progname }
	if -t {
		importas -i rundeps rundeps
		test -n "${rundeps}"
	}
	importas -isu rundeps rundeps
	redo-ifchange $rundeps
}
backtick hash { ../repo/get -h $progname }
backtick version { ../repo/get -k version $progname }
multisubstitute {
	importas -iu hash hash
	importas -iu version version
}
define PKGDIR "../cache/${hash}.${progname}#${version}"
export VENUS_CUR_PKGNAME "${progname}"
if { redo-ifchange $PKGDIR }
ln -s $PKGDIR $3
