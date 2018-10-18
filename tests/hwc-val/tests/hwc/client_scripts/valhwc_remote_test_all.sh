#!/system/bin/sh
#
# Libraries and global variables
source ${0%/*}/valhwc_common

cd $HWCVAL_DIR
killproc surfaceflinger
sleep 1

logcat -c
rm logcat_start_sf.log
logcat -f logcat_start_sf.log &
surfaceflinger&
sleep 1
hwclogviewer&
echo "Waiting for bootanimation..."
while [ `ps | grep -c bootanimation` -eq 0 ]
do
  sleep 1
done
killproc bootanimation
sleep 1
rm summary.txt
source valhwc_remote_run_test.sh valhwc_camera_test $@
source valhwc_remote_run_test.sh valhwc_dialog_test $@
source valhwc_remote_run_test.sh valhwc_gallery_test $@
source valhwc_remote_run_test.sh valhwc_game_test $@
source valhwc_remote_run_test.sh valhwc_home_test $@
source valhwc_remote_run_test.sh valhwc_lock_test $@
source valhwc_remote_run_test.sh valhwc_monitor_test $@
source valhwc_remote_run_test.sh valhwc_notification_test $@
source valhwc_remote_run_test.sh valhwc_nv12_video_full_test $@
source valhwc_remote_run_test.sh valhwc_nv12_video_part_test $@
source valhwc_remote_run_test.sh valhwc_recent_apps_test $@

