#!/bin/execlineb -P
elglob -s sets "config/*"
forx -Eo 0 set { $sets }
#export VENUS_CONFIG_FILE "../../${set}" # ../.. = modules/${mod}
backtick hash {
	pipeline { venus-cksum $set }
	venus-conf $set
}
backtick modules { venus-conf -t modules $set }
multisubstitute {
	importas -iu hash hash
	importas -isu modules modules
}
forx -Eo 0 mod { $modules }
# step 1 (files to populate)
backtick -Ex name { basename $set }
export VENUS_CONFIG_FILE "$name"
if { redo-ifchange modules/${mod}/${hash}.sys }
# populate (do not record its deps)
if { redo populate }
# step 2 (remap all at once)
if { redo-ifchange modules/${mod}/${name}.progs }
# broken symlinks to ${set}.progs: step1 = new files | step2 = orphans
# collect orphans
pipeline { redo-targets populate }
xargs -I "{}"
define file "{}"
if -t { test -L $file }
if -nt { test -e $file }
rm $file
