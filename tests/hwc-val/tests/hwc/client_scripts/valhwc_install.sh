#!/system/bin/sh
source ${0%/*}/valhwc_common
echo "$0 $@"

resize

${0%/*}/valhwc_disable_net.sh
${0%/*}/valhwc_install_shims.sh

sleep 1

killproc logcat
killproc hwclogviewer
killproc surfaceflinger

sleep 1

logcat -c
rm logcat_start_sf.log
#logcat -f logcat_start_sf.log &

surfaceflinger&
sf_proc=$!

sleep 2

killproc bootanimation
killproc $sf_proc

echo "Ready to run tests"

