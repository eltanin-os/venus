#!/bin/execlineb -S3
elglob -ms packages "./*"
getcwd -E pwd
backtick VENUS_ROOTDIR {
	backtick -Ex dir { dirname $pwd }
	echo ${dir}/root
}
forx -pE path { $packages }
if { test -d "${path}" }
cd $path
getcwd VENUS_CUR_TARGETDIR
export VENUS_CUR_PKGDIR "${pwd}/${path}"
find . ! -type d -exec
    define file "{}"
    redo-ifchange ../../root/${file}
    ;
