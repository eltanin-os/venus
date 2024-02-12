#!/bin/execlineb -P
elglob -s cachedirs "modules/*/cache"
forx -E dir { $cachedirs }
redo ${dir}/clean # do not record its deps
