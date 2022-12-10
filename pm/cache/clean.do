#!/bin/execlineb -S3
pipeline {
	pipeline { redo-targets }
	getcwd -E PWD
	grep "${PWD}/.*.pkg"
}
xargs rm
