#!/bin/bash
# test_browser.sh

BROWSER=$1
LIB_PATH="/home/school/Desktop/my_alloc/example/libmy_secmalloc.so"
LOG_FILE="report_file.log"

if [ -z "$BROWSER" ]; then
    echo "Usage: $0 <browser>"
    echo "Example: $0 firefox"
    exit 1
fi

MSM_OUTPUT=$LOG_FILE LD_PRELOAD=$LIB_PATH $BROWSER
