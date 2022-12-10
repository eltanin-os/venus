#!/bin/execlineb -S3
pipeline {
	pipeline { redo-targets }
	getcwd -E PWD
	grep "${PWD}"
}
xargs -I "{}"
    define target "{}"
    ifelse -Xn { test -d $target } { rm $target }
    backtick linkpath {
    	backtick -Ex file {
    		pipeline { basename $target }
    		sed -e "s;^.\\{129\\};;"  -e "s;#.*;;"
    	}
    	readlink -f ../programs/${file}
    }
    if -Xnt {
    	importas -iu linkpath linkpath
    	test "${target}" = "${linkpath}"
    }
    rm -Rf $target
