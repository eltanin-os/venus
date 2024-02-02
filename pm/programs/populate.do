#!/bin/execlineb -P
elglob -ms packages "*"
getcwd -E pwd
backtick VENUS_ROOTDIR {
       backtick -Ex dir { dirname $pwd }
       echo ${dir}/root
}
forx -E pkg { $packages }
if { test -d "${pkg}" }
cd $pkg
getcwd VENUS_CUR_TARGETDIR
export VENUS_CUR_PKGDIR "${pwd}/${pkg}"
find . ! -type d -exec execlineb -s0 -c "redo-ifchange ../../root/$@" {} +
