#!/bin/execlineb -S3
if { redo-ifchange ${1}.pkg }
if { mkdir $3 }
cd $3
pipeline { lzip -dc ../${1}.pkg }
venus-ar -x
