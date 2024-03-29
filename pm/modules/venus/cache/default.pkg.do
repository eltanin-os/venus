#!/bin/execlineb -S3
if {
	backtick arch { uname -m }
	backtick file {
		pipeline { echo $1 }
		sed -e "s;^.\\{129\\};;" -e "s;#;%23;"
	}
	backtick url {
		pipeline { ../repo/get -ct repo }
		pipeline {
			importas -i VENUS_CUR_REPO VENUS_CUR_REPO
			venus-conf -t $VENUS_CUR_REPO
		}
		venus-conf url
	}
	multisubstitute {
		importas -iu arch arch
		importas -iu file file
		importas -iu url url
	}
	curl -#Lo $3 ${url}/${arch}/${file}
}
backtick f1 {
	importas -i VENUS_CUR_PKGNAME VENUS_CUR_PKGNAME
	../repo/get -h $VENUS_CUR_PKGNAME
}
backtick f2 { pipeline { venus-cksum $3 } venus-conf $3 }
multisubstitute {
	importas -iu f1 f1
	importas -iu f2 f2
}
test "${f1}" = "${f2}"
