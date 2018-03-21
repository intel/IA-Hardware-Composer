#!/system/bin/sh
source ${0%/*}/valhwc_common

killproc logcat
logcat -c

args=$@
mangled_args=${args// /_}

logcat_filename="logcat_$mangled_args.txt"
logcat -v time -f $logcat_filename &

hwclog_filename="hwclog_$mangled_args.txt"
hwclogviewer > $hwclog_filename &

echo "simulating kernel crash"
reboot
echo running $1...
/system/bin/$@

killproc logcat
killproc hwclogviewer

ps | grep -q surfaceflinger
if [ $? != 0 ]
then
    echo ERROR: SurfaceFlinger crashed
    exit 1
fi
exit 0
