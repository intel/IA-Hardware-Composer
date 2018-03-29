#!/system/bin/sh
source ${0%/*}/valhwc_common

# usage -
# valhwc_test_repeat.sh <repeat_count> { test_all_command | run_test_command }
#
# repeats a test run <repeat_count> times. This script works with valhwc_test_all.sh and valhwc_run_test.sh
#

repeat=$1
shift
i=1
while [ $i -le $repeat ]
do
    echo
    echo test_repeat: running sequence $i of $repeat...
    echo
    # -run isn't understood by valhwc_test_all.sh or valhwc_run_test.sh, it is used to decorate the log output
    # filenames so that the repeat sequence is recorded as part of the filename
    $@ -run=$i
    if [ $? != 0 ]
    then
        echo ERROR: $@
        exit 1
    fi
    ps | grep -q surfaceflinger
    if [ $? != 0 ]
    then
        echo ERROR: SurfaceFlinger crashed
        exit 1
    fi
    let i=i+1
done
exit 0

