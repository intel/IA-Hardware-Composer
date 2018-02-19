#!/bin/bash

# This script assumes that it is run from the root
# of the weston directory and also that hwcomposer
# is cloned inside the root of weston's dir as iahwc
# these variables can be changed using --weston-dir and
# --iahwc-dir options to the script. see --help.

function print_help() {
    #fill in help
    echo "HELP"
    exit;
}

function build_iahwc() {
    # Build IAHWC
    pushd . > /dev/null
    cd $IAHWC_DIR
    git clean -xfd
    ./autogen.sh $AUTOGEN_CMDLINE --enable-gbm --enable-linux-frontend && \
        make -j$PARALLEL install
    popd
}

function build_weston() {
    # Build weston
    git clean -xfd
    ./autogen.sh $AUTOGEN_CMDLINE && \
        make -j$PARALLEL && \
        sudo make install
}

BKR=71c4f70e08faad6002ec8fe8cd1c7930bee8373b
AUTOGEN_CMDLINE=""
APPLY_PATCHES=0;
BUILD_IAHWC=0
BUILD_WESTON=0
PARALLEL=9
WESTON_DIR="$(pwd)"
IAHWC_DIR="iahwc"

for i in $@; do
    case $i in
        --apply-patches)
            APPLY_PATCHES=1;
            ;;
        --build)
            BUILD_IAHWC=1;
            BUILD_WESTON=1;
            ;;
        --build-iahwc)
            BUILD_IAHWC=1;
            ;;
        --build-weston)
            BUILD_WESTON=1;
            ;;
        --weston-dir=*)
            WESTON_DIR="${i//*=}";
            ;;
        --iahwc-dir=*)
            IAHWC_DIR="${i//*=}";
            ;;
        -j=* | --parallel=*)
            PARALLEL="${i//*=}";
            ;;
        -h | --help | *)
            print_help;
    esac
done

if [ ! -z ${WLD+x} ]; then
    echo "WLD found: $WLD"
    AUTOGEN_CMDLINE="--prefix=$WLD"
fi

WESTON_DIR=$(readlink -f $WESTON_DIR)

cd $WESTON_DIR

IAHWC_DIR=$(realpath --relative-to $WESTON_DIR $IAHWC_DIR)

PATCHES="$IAHWC_DIR/os/linux/weston/patches/*"
# checkout to weston best known revision and apply patches
if [ $APPLY_PATCHES -eq 1 ]; then
    git checkout $BKR
    sed -i "s|IAHWC_DIR|$IAHWC_DIR|" $PATCHES
    git am $PATCHES

    # reset patches to use IAHWC_DIR.
    pushd . > /dev/null
    cd $IAHWC_DIR
    git checkout os/linux/weston/patches
    popd
fi

[ $BUILD_IAHWC -eq 1 ] && build_iahwc;
[ $BUILD_WESTON -eq 1 ] && build_weston;
