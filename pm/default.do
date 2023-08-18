#!/bin/execlineb -S3
importas -iu VENUS_ROOTDIR VENUS_ROOTDIR
backtick -Ex path {
	pipeline { echo $3 }
	sed -e "s;${VENUS_ROOTDIR}/;;" -e "s;/[^/]*$;;"
}
backtick -Ex relpath {
	backtick rootdir {
		backtick -Ex dir { dirname $3 }
		cd $dir
		pwd -P
	}
	multisubstitute {
		importas -iu VENUS_CUR_PKGDIR VENUS_CUR_PKGDIR
		importas -iu rootdir rootdir
	}
	# generic function to convert absolute to relative path
	awk -vcurrent=${rootdir} -vtarget=${VENUS_CUR_PKGDIR}/${path} "
BEGIN {
	if (target == \".\") target = \"/\"
	target = \"/\" substr(target, 1 + index(target, \"/\"))
	if (current == \".\") current = \"/\"
	current = \"/\" substr(current, 1 + index(current, \"/\"))
	appendix = substr(target, 2)
	relative = \"\"
	while (1) {
		appendix = target
		sub(\"^\" current \"/\", \"\", appendix)
		if (!(current != \"/\" && appendix == target)) break
		if (current == appendix) {
			relative = (relative == \"\" ? \".\" : relative)
			sub(/^\\//, \"\", relative)
			print relative
			exit 0
		}
		sub(/\\/[^\\/]*\\/?$/, \"\", current)
		relative = relative (relative == \"\" ? \"\" : \"/\") \"..\"
	}
	sub(/^\\//, \"\", appendix)
	relative = relative (relative == \"\" ? \"\" : (appendix == \"\" ? \"\" : \"/\")) appendix
	print relative
}
"
}
# XXX: use a config file
case -- $path {
	"(boot|etc)(|/.*)" {
		if -Xnt { test -e ${VENUS_ROOTDIR}/${path}/${1} }
		importas -iu VENUS_CUR_TARGETDIR VENUS_CUR_TARGETDIR
		cp ${VENUS_CUR_TARGETDIR}/${path}/${1} $3
	}
}
ln -s ${relpath}/${1} $3
