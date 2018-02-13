#!/bin/bash

# This script assumes that it is run from the root
# of the weston directory and also that hwcomposer
# is cloned inside the root of weston's dir as iahwc

function build_iahwc() {

    # Build IAHWC
    pushd . > /dev/null
    cd iahwc
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
PATCHES="iahwc/os/linux/weston/patches/*"
APPLY_PATCHES=0;
BUILD_IAHWC=0
BUILD_WESTON=0
PARALLEL=9

if [ ! -z ${WLD+x} ]; then
    echo "WLD found: $WLD"
    AUTOGEN_CMDLINE="--prefix=$WLD"
fi

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
        -j=* | --parallel=*)
            PARALLEL="${i//*=}";
            ;;
    esac
done

# checkout to weston best known revision and apply patches
if [ $APPLY_PATCHES -eq 1 ]; then
    git checkout $BKR
    git apply --check $PATCHES && git am $PATCHES
fi

[ $BUILD_IAHWC -eq 1 ] && build_iahwc;
[ $BUILD_WESTON -eq 1 ] && build_weston;
