#!/system/bin/sh
#

# Libraries and global variables
source ${0%/*}/valhwc_common

cd $HWCVAL_DIR

# ... if you leave the echo until after the nohup command, the user never sees it.
echo "DISCONNECT NOW"
(nohup ./valhwc_run_harness.sh $* -sleep=10 > out_hwch.log) &
sleep 1

