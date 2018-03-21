#!/system/bin/sh
#
# Launches the tests execution


# Add busybox path to PATH for advanced unix commands
PATH=$PATH:/system/xbin/busybox:/system/xbin/busybox/busybox:/data/xbin/busybox


# Libraries and global variables
SCRIPT_DIR=${0%/*}
source $SCRIPT_DIR/valhwc_common

# Set directory to store test images
export HWCVAL_IMAGE_DIR=$HWCVAL_DIR/images

# Move to validation directory to execute tests
cd $HWCVAL_DIR

# Remove any old archive of SSIM failure images
rm -f dump.tgz
rm -rf dump

# Analyze input arguments
crcval=0
dpstwason=0
logcat_option="-vthreadtime -f"
hwclog_option="-v"
harness_args=

for i in $@
do
    if [[ $i == *crc* ]]
    then
        crcval=1
    elif [ "$i" == "-shortlog" ]
    then
        hwclog_option="-f" # remove verbose option
    elif [ "$i" == "-debug_mode" ]
    then
        DEBUG_MODE=1
    elif [[ "${i:0:8}" == "-hwclog=" ]]
    then
        hwclog_option="${i:8}"
        hwclog_option="${hwclog_option//_/ }"
    elif [[ "${i:0:7}" == "-sleep=" ]]
    then
        sleep ${i:7}
    else
        harness_args="$harness_args $i"
    fi
done

# Make /dev/input/* writable
# TODO: remove
saved_pwd=$PWD
cd /dev/input
ls | xargs chmod 666  # Note: "chmod 666 /dev/input/*" causes "No such file" errors
cd $saved_pwd

# Kill & remove old log files
killproc hwclogviewer
killproc logcat
killproc surfaceflinger
rm -f logcat_hwch.log hwclog_hwch.log kmsg_hwch.log
logcat -c

# Restart log file capture
logcat $logcat_option logcat_hwch.log &
logcat_proc=$!
#$SCRIPT_DIR/valhwc_kmsgReader.sh -ftrace > kmsg_hwch.log &
kmsg_proc=$!

if [[ "$hwclog_option" != "none" ]]
then
    #TODO Renable once the hwclogviewer is enabled in REAL HWC
    #hwclogviewer $hwclog_option > hwclog_hwch.log &
    hwclog_proc=$!
else
    hwclog_proc=""
fi

sleep 1

setprop intel.hwc.debuglogbufk 512

# Set any target-specific properties
device=`getprop ro.product.device`
if [ -e ${SCRIPT_DIR}/valhwc_${device}_config.sh ]
then
    source ${SCRIPT_DIR}/valhwc_${device}_config.sh
fi

if [ $crcval == 1 ]
then
    sleep 1
    echo "DPST setting prior to running the test..."
    ${SCRIPT_DIR}/valhwc_dpstcfg.sh status
    dpstwason=$?
    ${SCRIPT_DIR}/valhwc_dpstcfg.sh off
    ${SCRIPT_DIR}/valhwc_dpstcfg.sh dump
fi


# Run valhwcharness in debug or default mode
#
# LD_PRELOAD was added for M dessert, where we found that even when the DRM shim is properly
# installed, HWC's calls into DRM go straight into the real library, ignoring the shim.
# We suggest that bionic may have some sort of cache or other structure which it uses to
# determine which library to load without using the name. However, it's puzzling that
# even if we reboot the system with the shims installed and THEN run the test, we get
# the same problem, so if there is a cache it must be persistent.
#

if [ $DEBUG_MODE ]
then
    if [ -e /system/bin/gdbserver64 ]
    then
        LD_PRELOAD=$LIBDIR/libdrm.so gdbserver64 :5039 valhwcharness $harness_args
    else
        LD_PRELOAD=$LIBDIR/libdrm.so gdbserver :5039 valhwcharness $harness_args
    fi
else
    LD_PRELOAD=$LIBDIR/libdrm.so valhwcharness $harness_args
fi

# If a dump directory has been generated, then compress it for retrieval
if [ -d "dump" ]
then
    tar -czf dump.tgz dump
fi


if [ $crcval == 1 ]
then
    sleep 1
    ${SCRIPT_DIR}/valhwc_dpstcfg.sh status
    if [ $? == 1 ]
    then
        echo "*********************************************************************"
        echo "***** ERROR: DPST enabled during the test, CRC results invalid ******"
        echo "*********************************************************************"
    fi
    ${SCRIPT_DIR}/valhwc_dpstcfg.sh dump
    echo "Reinstating DPST to what it was prior to the test..."
    if [ $dpstwason == 1 ]
    then
        ${SCRIPT_DIR}/valhwc_dpstcfg.sh on
    else
        ${SCRIPT_DIR}/valhwc_dpstcfg.sh off
    fi

fi

# Kill pending processes
if [[ "$hwclog_proc" != "" ]]
then
    killproc $hwclog_proc
fi

killproc $kmsg_proc
killproc $logcat_proc


