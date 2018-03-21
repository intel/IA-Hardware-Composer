#!/system/bin/sh
#
##############################################################################
# Start the shims checks.
# Start collecting hwclogviewer and logcat log files.
# If called from the target itself, need to be called with ./
#
# Arguments:
#   -h/--h/help/-help/--help::= helper function
#   <arg>                   ::= <loglevel> | <systrace> | <crcvalidation>\n"
#   <loglevel>              ::= -log_pri={E|W|I|D|V}\n"
#   <systrace>              ::= -systrace[=<seconds>]\n"
#   <crcvalidation>         ::= {<hwcval> | <drmval>}"
#   <drmval>                ::= -crc\n"
# Returns:
#   0
##############################################################################

# Libraries and global variables
source ${0%/*}/valhwc_common

# Clean up of log files
cd $HWCVAL_DIR
rm -f hwclog_start_android.log
rm -f logcat_start_android.log
logcat -c

# Populate log files again
hwclogviewer -v -f > hwclog_start_android.log&
logcat -f logcat_start_android.log&

sleep 1

# Start checks
/vendor/bin/valhwc_util start $@

echo "Now please use Android"




