#!/system/bin/sh
#
# Configuration of dpst


source ${0%/*}/valhwc_common


# check whether it's a GMIN build of Android, or an MCG build
if [ -f "/d/dri/0/i915_dpst_status" ]
then
    #echo "Android target is a GMIN build of Android"
    GMIN_ANDROID=1
else
    #echo "Android target is an MCG build of Android"
    GMIN_ANDROID=0
fi

# check valhwc_dpstcfg syntax
#
if [ "$#" == "1" ]
then
    if [ "$1" == "on" ]
    then
        DPST_ON=1
    fi
    if [ "$1" == "off" ]
    then
        DPST_ON=0
    fi
    if [ "$1" == "dump" ]
    then
        DPST_DUMP=1
    fi
    if [ "$1" == "status" ]
    then
        DPST_STATUS=1
    fi
fi

if [ "$DPST_ON" == "" ] && [ "$DPST_DUMP" == "" ] && [ "$DPST_STATUS" == "" ]
then
    echo "Syntax error. Usage..."
    echo "From Android shell -"
    echo "> valhwc_dpstcfg.sh {on|off|dump|status}"
    echo "From devkit -"
    echo "> valdpst {on|off|dump|status}"
    echo
    exit 1
fi

TMPFIL=$HWCVAL_DIR/dpststatus.txt

# dpst_status
# return codes -
# 1 = DPST enabled
# 0 = DPST disabled
#
dpst_status()
{
    rc=0
    rm -f $TMPFIL

    if [ "$GMIN_ANDROID" == "1" ]
    then
        chown system:system /d/dri/0/i915_dpst_status
        if [ `cat /d/dri/0/i915_dpst_status | grep -c "histogram logic: enabled"` -ne 0 ]
        then
            rc=1
        else
            rc=0
        fi
    else
        chown system:system /d/dri/0/i915_dpst_api
        echo "DPST STATUS END" > /d/dri/0/i915_dpst_api
        cat /d/dri/0/i915_dpst_api > $TMPFIL
        if [ `grep -c "DPST Enabled: yes" $TMPFIL` -ne 0 ]
        then
            rc=1
        else
            rc=0
        fi
    fi

    rm -f $TMPFIL

    if [[ "$1" != "silent" ]]
    then
        if [[ $rc -eq 1 ]]
        then
            echo "DPST enabled"
        else
            echo "DPST disabled"
        fi
    fi
    return $rc
}

dpst_on()
{
    echo "Enabling DPST..."
    if [ "$GMIN_ANDROID" == "1" ]
    then
        chown system:system /d/dri/0/i915_dpst_disable
        chown system:system /d/dri/0/i915_dpst_status
        echo 0 > /d/dri/0/i915_dpst_disable
    else
        chown system:system /d/dri/0/i915_dpst_api
        echo "DPST ENABLE END" > /d/dri/0/i915_dpst_api
        cat /d/dri/0/i915_dpst_api
    fi
    dpst_status silent
    rc=$?
    return $rc
}

dpst_off()
{
    echo "Disabling DPST..."
    if [ "$GMIN_ANDROID" == "1" ]
    then
        chown system:system /d/dri/0/i915_dpst_disable
        chown system:system /d/dri/0/i915_dpst_status
        echo 1 > /d/dri/0/i915_dpst_disable
    else
        chown system:system /d/dri/0/i915_dpst_api

        echo "DPST DISABLE END" > /d/dri/0/i915_dpst_api
        cat /d/dri/0/i915_dpst_api
    fi
    dpst_status silent
    rc=$?
    return $rc
}

if [ "$DPST_ON" == "0" ]
then
    i=0
    imax=5
    dpst_status silent
    rc=$?
    while [ $i -lt $imax ]
    do
        if [ "$rc" == "0" ]
        then
            break
        elif [ $i -gt 0 ]
        then
            sleep 1
        fi
        i=$i+1
        dpst_off
        rc=$?
    done
    if [ $rc == "1" ]
    then
        echo "valhwc_dpstcfg.sh - ERROR: failed to turn off DPST"
    fi
fi

if [ "$DPST_ON" == "1" ]
then
    i=0
    imax=5
    dpst_status silent
    rc=$?
    while [ $i -lt imax ]
    do
        if [ "$rc" == "1" ]
        then
            break
        elif [ $i -gt 0 ]
        then
            sleep 1
        fi
        i=$i+1
        dpst_on
        rc=$?
    done
    if [ "$rc" == "0" ]
    then
        echo "valhwc_dpstcfg.sh - ERROR: failed to turn on DPST"
    fi
fi

if [ "$DPST_DUMP" == "1" ]
then
    if [ "$GMIN_ANDROID" == "1" ]
    then
        chown system:system /d/dri/0/i915_dpst_status
        cat /d/dri/0/i915_dpst_status
    else
        chown system:system /d/dri/0/i915_dpst_api
        echo "Dumping DPST Registers..."
        echo "DPST DUMP_REG END" > /d/dri/0/i915_dpst_api
        cat /d/dri/0/i915_dpst_api
    fi
fi

if [ "$DPST_STATUS" == "1" ]
then
    dpst_status
    rc=$?
    if [ "$rc" == "1" ]
    then
        exit 1
    else
        exit 0
    fi
fi

