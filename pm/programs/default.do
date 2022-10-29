#!/bin/execlineb -S3
if -Xn { test "all" = "${1}" }
backtick -Ex dbfile { ../repo/get $1 }
if { redo-ifchange $dbfile }
backtick name { ../repo/get -k name $1 }
backtick version { ../repo/get -k version $1 }
backtick -D "" rdeps { ../repo/get -t rundeps $1 }
multisubstitute {
	importas -iu name name
	importas -iu version version
	importas -isu rdeps rdeps
}
if { redo-ifchange $rdeps }
ifelse -Xn { test "${1}" = "${name}" }  {
	if { redo-ifchange $name }
	ln -s $name $3
}
if { redo-ifchange ../cache/${1}#${version} }
if { ln -s ../cache/${1}#${version} $3 }
getcwd -E PWD
backtick -D "" files { cd $3 find . ! -type d }
importas -isu files files
forx -Ep file { $files }
backtick -Ex dir { dirname $file }
if { mkdir -p ../root/${dir} }
export VENUS_CUR_TARGET "${PWD}/${1}/${file}"
redo-ifchange ../root/${file}
