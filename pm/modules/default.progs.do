#!/bin/execlineb -S3
importas -i VENUS_CONFIG_FILE VENUS_CONFIG_FILE
backtick -Ex hash {
	pipeline { venus-cksum ../../config/${VENUS_CONFIG_FILE} }
	venus-conf ../../config/${VENUS_CONFIG_FILE}
}
if { redo-ifchange ${hash}.sys }
ln -s ${hash}.sys $3
