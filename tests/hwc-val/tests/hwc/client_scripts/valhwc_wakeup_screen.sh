#!/system/bin/sh
# ****************************************************************************
#
# Copyright (c) Intel Corporation 2013.
#
# DISCLAIMER OF WARRANTY
# NEITHER INTEL NOR ITS SUPPLIERS MAKE ANY REPRESENTATION OR WARRANTY OR
# CONDITION OF ANY KIND WHETHER EXPRESS OR IMPLIED (EITHER IN FACT OR BY
# OPERATION OF LAW) WITH RESPECT TO THE SOURCE CODE.  INTEL AND ITS SUPPLIERS
# EXPRESSLY DISCLAIM ALL WARRANTIES OR CONDITIONS OF MERCHANTABILITY OR
# FITNESS FOR A PARTICULAR PURPOSE.  INTEL AND ITS SUPPLIERS DO NOT WARRANT
# THAT THE SOURCE CODE IS ERROR-FREE OR THAT OPERATION OF THE SOURCE CODE WILL
# BE SECURE OR UNINTERRUPTED AND HEREBY DISCLAIM ANY AND ALL LIABILITY ON
# ACCOUNT THEREOF.  THERE IS ALSO NO IMPLIED WARRANTY OF NON-INFRINGEMENT.
# SOURCE CODE IS LICENSED TO LICENSEE ON AN "AS IS" BASIS AND NEITHER INTEL
# NOR ITS SUPPLIERS WILL PROVIDE ANY SUPPORT, ASSISTANCE, INSTALLATION,
# TRAINING OR OTHER SERVICES.  INTEL AND ITS SUPPLIERS WILL NOT PROVIDE ANY
# UPDATES, ENHANCEMENTS OR EXTENSIONS.
#
# File Name:      valhwc_wakeup_screen.sh
#
# Description:
# Util function to wake up the device and unlock the display
#
# Notes:
#
# ****************************************************************************


source ${0%/*}/valhwc_common


ANDROID_ON=`ps | grep systemui | grep -vc grep`
if [ "$ANDROID_ON" -ne "0" ]
then
    STATE=`dumpsys power | grep -c "mWakefulness=Asleep"`
    if [ $STATE -ne 0 ]
    then
        echo "Screen Off - Waking Up"
        # Send a KEYCODE_POWER (WakeUp)
        /system/bin/input keyevent 26
        # Send a KEYCODE_MENU (Unlock)
        /system/bin/input keyevent 82
    else
        echo "Screen already On"
    fi
else
    echo "Android is OFF"
fi
