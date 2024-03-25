#!/bin/execlineb -S3
if { mkdir $3 }
importas -i VENUS_CONFIG_FILE VENUS_CONFIG_FILE
backtick repositories { repo/get -clt repo }
importas -isu repositories repositories
forx -Eo 0 repo { packages $repositories }
if {
	if -nt { test "packages" = "${repo}" }
	redo-ifchange repo/${repo}
}
export VENUS_CUR_REPO "${repo}"
backtick -D "" packages { repo/get -clt $repo }
importas -isu packages packages
forx -Eo 0 pkg { $packages }
backtick -D "latest" version {
	pipeline { repo/get -ct $repo }
	pipeline { venus-conf $pkg }
	venus-conf version
}
importas -iu version version
redo ${3}/${pkg}#${version}
