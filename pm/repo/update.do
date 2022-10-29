#!/bin/execlineb -S3
# XXX: repo.conf (repoXXX{} list) := symlink -> ${reponame}.${sum} (atomicity)
if { redo-always }
elglob repositories "*"
forx -Ep repo { $repositories }
if { test -d "${repo}" }
cd $repo
backtick arch { uname -m }
backtick safeurl { venus-conf safeurl repo.conf }
backtick tmpfile { mktemp }
multisubstitute {
	importas -iu arch arch
	importas -iu safeurl safeurl
	importas -iu tmpfile tmpfile
}
if { curl -#Lo chksum ${safeurl}/${arch}/chksum }
if { curl -#Lo $tmpfile ${safeurl}/${arch}/database }
backtick f1sum { pipeline { venus-cksum -w $tmpfile } venus-conf $tmpfile }
backtick -D "0xF" f2sum { if { test -e database } venus-conf database database }
multisubstitute {
	importas -iu f1sum f1sum
	importas -iu f2sum f2sum
}
ifelse { test "${f1sum}" = "${f2sum}" } { exit 0 }
if { pipeline { lzip -dc $tmpfile } venus-ar -x }
redirfd -w 1 database
echo database:${f1}
