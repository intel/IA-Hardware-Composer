#!/system/bin/sh
#
##############################################################################
# Uninstall shims from run-time locations
#
# Arguments:
#   None
#
# Returns:
#   None
##############################################################################

# Libraries and global variables
source ${0%/*}/valhwc_common


##############################################################################
# Uninstall the shims of the libraries. Clean up file system (for PAVE only)
# Arguments:
#   $1: directory where real libraries are (/vendor/libXX or /system/libXX)
# Returns:
#   None
##############################################################################
uninstall_shims()
{
    if [ -e $1 ]
    then
        echo "Uninstalling shims in $1"
        if [ -e $1/libdrm.real.so ]
        then
            val_move $1/libdrm.real.so $1/libdrm.so
        fi
        if [ -e $1/hw/hwcomposer.real.so ]
        then
            val_move $1/hw/hwcomposer.real.so $1/hw/hwcomposer.$TARGETSHIM.so
        fi

    fi
}


mount -o rw,remount /vendor

# Stop OS before copying .so libraries
android_process_count=`ps | grep -c android`
if [ $android_process_count -gt 0 ]
then
     stop
fi


# Kill pending processes
killproc surfaceflinger
killproc logcat
killproc hwclogviewer

sleep 5

# Clean up system for 64 and 32 bit builds
if [ -e /vendor/lib64 ]
then
    uninstall_shims /vendor/lib64
fi
uninstall_shims /vendor/lib

val_move  /system/bin/netd.d /system/bin/netd
val_move  /system/bin/wificond.d /system/bin/wificond

# drm shim removed -> restart coreu
stop media
start media

