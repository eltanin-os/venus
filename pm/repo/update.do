#!/bin/execlineb -S3
# XXX: repo.conf (repoXXX{} list) := symlink -> ${reponame}.${sum} (atomicity)
if { redo-always }
elglob repositories "*"
forx -Ep repo { $repositories }
if { test -f "${repo}/repo.conf" }
cd $repo
backtick -Ex tmpfile { mktemp }
if {
	backtick arch { uname -m }
	backtick safeurl { venus-conf safeurl repo.conf }
	multisubstitute {
		importas -iu arch arch
		importas -iu safeurl safeurl
	}
	if { curl -#Lo chksum ${safeurl}/${arch}/chksum }
	curl -#Lo $tmpfile ${safeurl}/${arch}/database
}
ifelse {
	backtick sum1 {
		pipeline { venus-cksum -w $tmpfile }
		venus-conf $tmpfile
	}
	backtick -D "0xF" sum2 {
		if { test -e database }
		venus-conf database database
	}
	multisubstitute {
		importas -iu sum1 sum1
		importas -iu sum2 sum2
	}
	test "${sum1}" = "${sum2}"
}
{ exit 0 }
if { pipeline { lzip -dc $tmpfile } venus-ar -x }
redirfd -w 1 database
echo database:${f1}
