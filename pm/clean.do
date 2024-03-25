#!/bin/execlineb -P
elglob -s cachedirs "modules/*/cache"
redo ${cachedirs}/clean # do not record its deps
