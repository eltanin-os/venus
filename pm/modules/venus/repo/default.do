#!/bin/execlineb -S3
if {
	importas -i VENUS_CONFIG_FILE
	redo-ifchange ../../../config/${VENUS_CONFIG_FILE}
}
if { mkdir $3 }
cd $3
if {
	backtick arch { uname -m }
	backtick safeurl {
		pipeline { ../get -ct repo }
		pipeline {
			importas -i VENUS_CUR_REPO VENUS_CUR_REPO
			venus-conf -t $VENUS_CUR_REPO
		}
		venus-conf safeurl
	}
	multisubstitute {
		importas -iu arch arch
		importas -iu safeurl safeurl
	}
	if { curl -#Lo chksum ${safeurl}/${arch}/chksum }
	curl -#Lo database.vlz ${safeurl}/${arch}/database
}
pipeline { plzip -dc database.vlz }
venus-ar -x
