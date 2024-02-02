#!/bin/execlineb -S3
importas -iu VENUS_CUR_PKGDIR VENUS_CUR_PKGDIR
if { redo-ifchange $VENUS_CUR_PKGDIR }
importas -iu VENUS_ROOTDIR VENUS_ROOTDIR
backtick -Ex path {
	pipeline { echo $3 }
	sed -e "s;${VENUS_ROOTDIR}/;;" -e "s;/[^/]*$;;"
}
# XXX: use a config file
case -- $path {
	"(boot|etc)(|/.*)" {
		if -nt { test -e ${VENUS_ROOTDIR}/${path}/${1} }
		importas -iu VENUS_CUR_TARGETDIR VENUS_CUR_TARGETDIR
		cp ${VENUS_CUR_TARGETDIR}/${path}/${1} $3
	}
}
backtick -Ex relpath {
	backtick rootdir {
		backtick -Ex dir { dirname $3 }
		cd $dir
		pwd -P
	}
	backtick storedir { dirname $VENUS_ROOTDIR }
	multisubstitute {
		importas -iu rootdir rootdir
		importas -iu storedir storedir
	}
	awk
	    -f ${storedir}/realpath.awk
	    -vcurrent=${rootdir}
	    -vtarget=${VENUS_CUR_PKGDIR}${path}
}
ln -s ${relpath}/${1} $3
