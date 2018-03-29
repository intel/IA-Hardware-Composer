#!/system/bin/sh
#
# Runs all the hwc validation legacy tests

# Libraries and global variables
source ${0%/*}/valhwc_common

rm logcat_val*
rm hwclog_val*

checksf(){
    ps | grep -q surfaceflinger
    if [ $? != 0 ]
    then
        echo "WARNING: SurfaceFlinger crashed. Restarted."
        surfaceflinger&
        sleep 2
    fi
    killproc bootanimation
}

if [ "$HWCVAL_SLEEP" = "" ]
then
    HWCVAL_SLEEP=0
fi
echo sleeping for $HWCVAL_SLEEP seconds between tests

./valhwc_run_test.sh valhwc_camera_test $@
checksf

sleep $HWCVAL_SLEEP
./valhwc_run_test.sh valhwc_dialog_test $@
checksf

sleep $HWCVAL_SLEEP
./valhwc_run_test.sh valhwc_gallery_test $@
checksf

sleep $HWCVAL_SLEEP
./valhwc_run_test.sh valhwc_game_test $@
checksf

sleep $HWCVAL_SLEEP
./valhwc_run_test.sh valhwc_home_test $@
checksf

sleep $HWCVAL_SLEEP
./valhwc_run_test.sh valhwc_lock_test $@
checksf

sleep $HWCVAL_SLEEP
./valhwc_run_test.sh valhwc_monitor_test $@
checksf

sleep $HWCVAL_SLEEP
./valhwc_run_test.sh valhwc_notification_test $@
checksf

sleep $HWCVAL_SLEEP
./valhwc_run_test.sh valhwc_nv12_video_full_test $@
checksf

sleep $HWCVAL_SLEEP
./valhwc_run_test.sh valhwc_nv12_video_part_test $@
checksf

sleep $HWCVAL_SLEEP
./valhwc_run_test.sh valhwc_recent_apps_test $@
checksf

exit 0

