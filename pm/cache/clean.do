#!/bin/execlineb -S3
backtick -Ex programs {
	pipeline { ls -l ../programs }
	sed -n "s;.*-> ;;p"
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
