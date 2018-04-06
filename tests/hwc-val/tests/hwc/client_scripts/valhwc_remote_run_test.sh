#!/system/bin/sh
source ${0%/*}/valhwc_common

args=$@
mangled_args=${args// /_}

logcat -c
rm logcat_$mangled_args.txt
logcat -f logcat_$mangled_args.txt &
echo "+++++++++++++++++++++++++++++++++++++++++ RUNNING $1 $2 +++++++++++++++++++++++++++++++++++++++++++++"
/system/bin/$@ >> summary.txt
killproc logcat
echo "+++++++++++++++++++++++++++++++++++++++++ FINISHED $1 $2 +++++++++++++++++++++++++++++++++++++++++++++"
echo


