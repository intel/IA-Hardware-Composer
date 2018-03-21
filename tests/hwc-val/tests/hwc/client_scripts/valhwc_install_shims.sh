#!/system/bin/sh
#
##############################################################################
# Install shims into run-time locations
#
# Arguments:
# no_sf = no surface flinger shim will be installed (recommended to use the harness).
# If no arg is provided, then SF shim will be installed (recommended
# with valstart/valstop or tests under Android)
#
# Returns:
#   0
##############################################################################

# Libraries and global variables
source ${0%/*}/valhwc_common


##############################################################################
# Install the shims for the libraries.
# Arguments:
#   $1: directory of the shims & libraries they are replacing
# Returns:
#   None
##############################################################################
install_library_shims()
{
    # Install HWC shim if not already installed
    if [ -e $1/hw/hwcomposer.$TARGETSHIM.so ] && [ -e $1/valhwc_composershim.so ]
    then
        hwc_is_shim=`grep -c valhwc_composershim $1/hw/hwcomposer.$TARGETSHIM.so`
        if [[ $hwc_is_shim -eq 0 ]]
        then
            # Save real HWC library
            val_copy  $1/hw/hwcomposer.$TARGETSHIM.so $1/hw/hwcomposer.real.so
            # Install vendor shims in place of real libraries
            val_copy $1/valhwc_composershim.so $1/hw/hwcomposer.$TARGETSHIM.so
        fi
    else
        echo "*** ERROR valhwc_install_shims is unable to install HWC shim ***"
        if [[ ! -e $1/hw/hwcomposer.$TARGETSHIM.so ]]
        then
            echo "*** ERROR $1/hw/hwcomposer.$TARGETSHIM.so does not exist ***"
        fi
        if [[ ! -e $1/valhwc_composershim.so ]]
        then
            echo "*** ERROR $1/valhwc_composershim.so does not exist ***"
        fi
        exit 1;
    fi
    # Install DRM shim if not already installed
    if [ -e $1/libdrm.so ] && [ -e $1/libvalhwc_drmshim.so ]
    then

        drm_is_shim=`grep -c drmshim $1/libdrm.so`
        if [[ $drm_is_shim -eq 0 ]]
        then
            # Save real library
            val_copy $1/libdrm.so $1/libdrm.real.so
            # Install shims in place of real libraries
            val_copy $1/libvalhwc_drmshim.so $1/libdrm.so
        fi
    else
        echo "*** ERROR valhwc_install_shims is unable to install DRM shim ***"
        if [[ ! -e $1/libdrm.so ]]
        then
            echo "*** ERROR $1/libdrm.so does not exist ***"
        fi
        if [[ ! -e $1/libvalhwc_drmshim.so ]]
        then
            echo "*** ERROR $1/libvalhwc_drmshim.so does not exist ***"
        fi
        exit 1;
    fi

}

##############################################################################
# Start script
##############################################################################

mount -o rw,remount /system

# Create validation directory tree if not present and set variable to store images
export HWCVAL_IMAGE_DIR=$HWCVAL_DIR/images
mkdir -p $HWCVAL_IMAGE_DIR

cd $HWCVAL_DIR

SF_STATE="`getprop init.svc.surfaceflinger`"
if [[ $SF_STATE != "stopped" ]]
then
    # Wake up the device and unlock the display before running the test
    ${0%/*}/valhwc_wakeup_screen.sh


    # Stop OS before copying .so libraries
    echo "Stopping Android..."
    stop

    sleep 5

    # I've seen surfaceflinger still running after "stop".
    # So this is to make sure, though it should be unnecessary.
    ps | grep surfaceflinger
    while [ $? -eq 0 ]
    do
        stop
        killproc surfaceflinger
        sleep 5
    done
fi


${0%/*}/valhwc_disable_net.sh
# Install shims based on type.
if [ -e /vendor/lib64 ]
then
    install_library_shims /vendor/lib64
else
    install_library_shims /vendor/lib
fi



# If drm shim installed -> restart coreu

# Disable SELinux
setenforce 0

stop media

sync
sync
sync

start media
