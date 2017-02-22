#!/bin/bash

#  generate.sh
#  Shiki
#
#  Copyright © 2016-2017 vit9696. All rights reserved.

ret=0

rm -f "${PROJECT_DIR}/Shiki/kern_resources.cpp"

"${TARGET_BUILD_DIR}/ResourceConverter" \
	"${PROJECT_DIR}/Resources" \
	"${PROJECT_DIR}/Shiki/kern_resources.cpp" \
	"${PROJECT_DIR}/Shiki/kern_resources.hpp" || ret=1

if (( $ret )); then
	echo "Failed to build kern_resources.cpp"
	exit 1
fi
