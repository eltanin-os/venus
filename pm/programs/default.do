#!/bin/execlineb -S3
if -Xn { test "all" = "${1}" }
if {
	backtick -Ex dbfile { ../repo/get $1 }
	redo-ifchange $dbfile
}
if {
	pipeline { ../repo/get -t rundeps $1 }
	xargs redo-ifchange
}
backtick -Ex name { ../repo/get -k name $1 }
ifelse -Xn { test "${1}" = "${name}" }  {
	if { redo-ifchange $name }
	ln -s $name $3
}
getcwd PWD
backtick hash { ../repo/get -h $name }
backtick version { ../repo/get -k version $1 }
multisubstitute {
	importas -iu PWD PWD
	importas -iu hash hash
	importas -iu version version
}
define PKGDIR "../cache/${hash}.${1}#${version}"
export VENUS_CUR_PKGNAME "${name}"
if { redo-ifchange $PKGDIR }
if { ln -s $PKGDIR $3 }
cd $3
find . ! -type d -exec
    define file "{}"
    backtick -Ex dir { dirname $file }
    if { mkdir -p ../../root/${dir} }
    export VENUS_CUR_DIR "${dir}"
    export VENUS_CUR_FILE "${PWD}/${PKGDIR}/${file}"
    export VENUS_CUR_TARGET "${PWD}/${1}/${file}"
    redo-ifchange ../../root/${file}
    ;
