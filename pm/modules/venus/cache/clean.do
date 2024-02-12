#!/bin/execlineb -S3
backtick -Ex programs {
	pipeline {
		elglob -s directories "../*.progs"
		ls -l ${directories}/
	}
	pipeline { sed -n "s;.*-> ;;p" }
	pipeline { sort }
	uniq
}
pipeline { ls }
xargs -I "{}"
    define target "{}"
    case -- "\n${programs}\n" {
    	".*\n../cache/${target}\n.*" { exit 0 }
    }
    case -- $target {
    	".*.do" { exit 0 }
    }
    rm -Rf $target
