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
if {
	pipeline { ../repo/get -t rundeps $progname }
	xargs redo-ifchange
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
getcwd PWD
backtick hash { ../repo/get -h $progname }
backtick version { ../repo/get -k version $progname }
multisubstitute {
	importas -iu PWD PWD
	importas -iu hash hash
	importas -iu version version
}
define PKGDIR "../cache/${hash}.${progname}#${version}"
export VENUS_CUR_PKGNAME "${progname}"
if { redo-ifchange $PKGDIR }
if { ln -s $PKGDIR $3 }
cd $3
find . ! -type d -exec
    define file "{}"
    backtick -Ex dir { dirname $file }
    if { mkdir -p ../../root/${dir} }
    export VENUS_CUR_DIR "${dir}"
    export VENUS_CUR_FILE "${PWD}/${PKGDIR}/${file}"
    export VENUS_CUR_TARGET "${PWD}/${progname}/${file}"
    redo-ifchange ../../root/${file}
    ;
