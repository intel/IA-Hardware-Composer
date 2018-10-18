#!/system/bin/sh
#
##############################################################################
# Install the shims if not already present and start Android.
# If called from the target itself, need to be called with ./
#
# Arguments:
#   0
# Returns:
#   0
##############################################################################

# Libraries and global variables
source ${0%/*}/valhwc_common

# Ensure to have read/write access
mount -o rw,remount /system

# Create validation directory tree if not present
mkdir -p $HWCVAL_DIR
cd $HWCVAL_DIR

# Some clean up
rm -rf dump

sleep 1

# Test to see if the shims are running
drm_shim_installed=`grep -c drmshim $LIBDIR/libdrm.so`
hwc_shim_installed=`grep -c valhwc_composershim $LIBDIR/hw/hwcomposer.android_ia.so`
sf_shim_installed=`grep -c surfaceflingershim /system/bin/surfaceflinger`
if [[ $drm_shim_installed == 0 || $hwc_shim_installed == 0 || $sf_shim_installed == 0 ]]
then
    shims_installed=0
else
    shims_installed=1
fi

setprop intel.hwc.debuglogbufk 512

# surfaceflinger will be run as the system user, make sure it can access the
# CRC drivers in debugfs
echo "Changing ownership of the display CRC drivers"
chown system:system /sys/kernel/debug/dri/0/i915_display_crc_ctl
chown system:system /sys/kernel/debug/dri/0/i915_pipe_*

# Install shims if not installed already
if [ $shims_installed -eq 0 ]
then
  echo "Installing shims"
  stop
  ./valhwc_install_shims.sh
  sleep 1
else
  echo "Shims already running"
  ./valhwc_killapp.sh hwclogviewer
  ./valhwc_killapp.sh logcat
fi

# Disable SELinux to avert permissions problems starting HWC validation service
echo 0 > /sys/fs/selinux/enforce

# Restart Android
android_process_count=`ps | grep -c android`
if [ $android_process_count -lt 2 ]
then
  echo "Restarting Android..."

  # Disable SELinux
  setenforce 0

  start
fi
