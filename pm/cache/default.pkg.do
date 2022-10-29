#!/bin/execlineb -S3
backtick -Ex name { pipeline { echo $1 } cut -d"#" -f1 }
backtick arch { uname -m }
backtick suffix { pipeline { echo $1 } cut -d"#" -f2 }
backtick url { ../repo/get -c url $name }
multisubstitute {
	importas -iu arch arch
	importas -iu suffix suffix
	importas -iu url url
}
if { curl -#Lo $3 ${url}/${arch}/${name}%23${suffix} }
backtick f1 { ../repo/get -h $name }
backtick f2 { pipeline { venus-cksum -w $3 } venus-conf $3 }
multisubstitute {
	importas -iu f1 f1
	importas -iu f2 f2
}
test "${f1}" = "${f2}"
