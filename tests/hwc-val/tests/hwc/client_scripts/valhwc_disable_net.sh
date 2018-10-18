#!/system/bin/sh
#
##############################################################################
# move net into run-time locations
#
# Arguments:
#
# Returns:
#   0
##############################################################################

# Libraries and global variables
source ${0%/*}/valhwc_common

##############################################################################
# Start script
##############################################################################

# move netd and wificond.
val_move  /system/bin/netd /system/bin/netd.d
val_move  /system/bin/wificond /system/bin/wificond.d
