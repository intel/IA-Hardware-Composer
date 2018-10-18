#/bin/bash
#
# Tests the crc pipe


source ${0%/*}/valhwc_common


##############################################################################
# Function to read crc value
# Arguments:
#   $1: id of the pipe to read crc values from
# Returns:
#   0
##############################################################################
readcrc()
{
    if [[ $1 != "A" && $1 != "B" && $1 != "C" ]]
    then
        echo "Missing parameter, syntax is -"
        echo "# readcrc {A|B|C}"
        return 1
    fi
    cat /d/dri/0/i915_pipe_$1_crc &
    catpee=$!
    sleep 1
    kill $catpee
    return 0
}


##############################################################################
# Function which sets the crc value
# Arguments:
#   $1: id of the pipe to read crc values from
#   $2: entries in the crc control register
# Returns:
#   0
##############################################################################
setcrc()
{
    # local doesn't work with array (in GMIN shell, anyway), so prefix array with function name
    setcrc_srcarray=("none" "plane1" "plane2" "pf" "pipe" "TV" "DP-B" "DP-C" "DP-D" "HDMI-B" "HDMI-C" "auto")
    local err=0

    if [[ $1 != "A" && $1 != "B" && $1 != "C" ]]
    then
        echo "Invalid pipe"
        err=1
    fi

    if [[ $err == 0 ]]
    then
        err=1
        for s in "${setcrc_srcarray[@]}"
        do
            if [[ $2 == $s ]]
            then
                err=0
                break
            fi
        done
        if [[ $err == 1 ]]
        then
            echo "Invalid source"
        fi
    fi

    if [[ $err == 1 ]]
    then
        echo "Missing parameter, syntax is -"
        echo "# setcrc {A|B|C} <source>"
        echo "\t<source> is -"
        for s in "${setcrc_srcarray[@]}"
        do
            echo "\t$s"
        done
        return 1
    fi

    echo "echo \"pipe $1 $2\" > /d/dri/0/i915_display_crc_ctl"
    echo "pipe $1 $2" > /d/dri/0/i915_display_crc_ctl

    if [[ -e /system/bin/gttmem ]]
    then
        # dump the CRC control register
        if [[ $1 == "A" ]]
        then
            gttmem -rd 0x60050
        fi
        if [[ $1 == "B" ]]
        then
            gttmem -rd 0x61050
        fi
        if [[ $1 == "C" ]]
        then
            gttmem -rd 0x62050
        fi
    fi

    return 0
}


if [[ $1 != "A" && $1 != "B" && $1 != "C" ]]
then
    echo "Missing parameter, syntax is -"
    echo "# $0 {A|B|C}"
    exit 1
fi

#srcarray=("plane1" "plane2" "pf" "pipe" "TV" "DP-B" "DP-C" "DP-D" "HDMI-B" "HDMI-C" "auto")
srcarray=("auto")

# disable CRC results - it is not possible to switch sources without going through source="none"
setcrc $1 none

for s in "${srcarray[@]}"
do
    if [[ $s == "none" ]]
    then
        continue
    fi

    echo "pipe $1 $s...................................................."

    # start CRC output from the selected source
    setcrc $1 $s

    readcrc $1

    setcrc $1 none
done