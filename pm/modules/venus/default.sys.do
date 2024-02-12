#!/bin/execlineb -S3
if { mkdir $3 }
importas -i VENUS_CONFIG_FILE VENUS_CONFIG_FILE
backtick packages {
	pipeline { venus-conf -t venus ../../config/${VENUS_CONFIG_FILE} }
	venus-conf -lt packages
}
importas -isu packages packages
forx -Eo 0 pkg { $packages }
backtick -D "latest" version {
	pipeline { venus-conf -t venus ../../config/${VENUS_CONFIG_FILE} }
	pipeline { venus-conf $pkg }
	venus-conf version
}
importas -iu version version
redo ${3}/${pkg}#${version}
