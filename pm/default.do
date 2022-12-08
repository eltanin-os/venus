#!/bin/execlineb -S3
multisubstitute {
	importas -i VENUS_CUR_DIR VENUS_CUR_DIR
	importas -i VENUS_CUR_FILE VENUS_CUR_FILE
	importas -i VENUS_CUR_TARGET VENUS_CUR_TARGET
}
# XXX: use a config file
case -- ${VENUS_CUR_DIR} {
	"[.](/boot|/etc)(|/.*)" {
		cp ${VENUS_CUR_FILE} $3
	}
}
ln -s ${VENUS_CUR_TARGET} $3
