#!/bin/execlineb -P
getcwd pwd
multisubstitute {
	elglob -s sets "config/*"
	importas -iu pwd pwd
}
forx -Eo 0 set { $sets }
backtick hash {
	pipeline { venus-cksum $set }
	venus-conf $set
}
backtick modules { venus-conf -t modules $set }
multisubstitute {
	importas -iu hash hash
	importas -isu modules modules
}
forx -Eo 0 mod { $modules }
backtick -Ex name { basename ${set} }
export VENUS_ROOTDIR "${pwd}/sys/${name}"
cd modules/${mod}/${hash}.sys
elglob -s packages "*"
forx -Eo 0 pkg { $packages }
cd $pkg
export VENUS_CUR_PKGABS "${pwd}/modules/${mod}/${hash}.sys/${pkg}"
export VENUS_CUR_PKGDIR "${pwd}/modules/${mod}/${name}.progs/${pkg}"
find . ! -type d -exec
    execlineb -s0 -c "cd \"${pwd}\" redo-ifchange sys/${name}/$@" {} +
