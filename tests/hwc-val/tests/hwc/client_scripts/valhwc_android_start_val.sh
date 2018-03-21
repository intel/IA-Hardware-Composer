#!/system/bin/sh
source ${0%/*}/valhwc_common

#
# client_scripts/valhwc_remote_start_checks.sh is the remote version of this
#

resize
killproc logcat
killproc hwclogviewer

./valhwc_install_shims.sh

killproc surfaceflinger

sleep 1

logcat -c
rm logcat_start_android.log
logcat -f logcat_start_android.log &

start

hwclogviewer > hwclog_start_android.log &

sleep 1

/system/vendor/bin/valhwc_util start

echo "Now please use Android"
