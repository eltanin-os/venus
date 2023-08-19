#!/bin/execlineb -S3
backtick -Ex progname { pipeline { echo $1 } cut -d"#" -f1 }
case -- $1 {
	"all" { exit 0 }
	# TODO: version check "name" = "name#latest"
	".*#.*" { redo-ifchange $progname }
}
if {
	backtick -Ex dbfile { ../repo/get $progname }
	redo-ifchange $dbfile
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
	pipeline { ../repo/get -t rundeps $progname }
	xargs redo-ifchange
}
if {
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
}
getcwd -E pwd
cd $3
backtick VENUS_ROOTDIR {
	backtick -Ex dir { dirname $pwd }
	echo ${dir}/root
}
getcwd VENUS_CUR_TARGETDIR
export VENUS_CUR_PKGDIR "${pwd}/${progname}"
find . ! -type d -exec
    define file "{}"
    redo-ifchange ../../root/${file}
    ;
