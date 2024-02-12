#!/bin/execlineb -P
# broken symlinks = orphans (after state change)
pipeline { redo-targets populate }
xargs -I "{}"
define file "{}"
if -t { test -L $file }
if -nt { test -e $file }
rm $file
