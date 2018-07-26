#!/bin/bash

# This script assumes that it is run from the root
# of the weston directory and also that hwcomposer
# is cloned inside the root of weston's dir as iahwc
# these variables can be changed using --weston-dir and
# --iahwc-dir options to the script. see --help.

# copied from the grub-mkconfig_lib script
print_option_help () {
    if test x$print_option_help_wc = x; then
        if wc -L  </dev/null > /dev/null 2>&1; then
            print_option_help_wc=-L
        elif wc -m  </dev/null > /dev/null 2>&1; then
            print_option_help_wc=-m
        else
            print_option_help_wc=-b
        fi
    fi
    if test x$grub_have_fmt = x; then
        if fmt -w 40  </dev/null > /dev/null 2>&1; then
            grub_have_fmt=y;
        else
            grub_have_fmt=n;
        fi
    fi
    print_option_help_lead="  $1"
    print_option_help_lspace="$(echo "$print_option_help_lead" | wc $print_option_help_wc)"
    print_option_help_fill="$((26 - print_option_help_lspace))"
    printf "%s" "$print_option_help_lead"
    if test $print_option_help_fill -le 0; then
        print_option_help_nl=y
        echo
    else
        print_option_help_i=0;
        while test $print_option_help_i -lt $print_option_help_fill; do
            printf " "
            print_option_help_i=$((print_option_help_i+1))
        done
        print_option_help_nl=n
    fi
    if test x$grub_have_fmt = xy; then
        print_option_help_split="$(echo "$2" | fmt -w 50)"
    else
        print_option_help_split="$2"
    fi
    if test x$print_option_help_nl = xy; then
        echo "$print_option_help_split" | awk \
                                              '{ print "                          " $0; }'
    else
        echo "$print_option_help_split" | awk 'BEGIN   { n = 0 }
  { if (n == 1) print "                          " $0; else print $0; n = 1 ; }'
    fi
}

function print_help() {
    self=`basename $0`

    gettext "Usage: ${self} [OPTION]"; echo
    gettext "Build iahwc and weston"
    echo
    print_option_help "--apply-patches" "apply all the required patches on weston"
    print_option_help "--build-iahwc" "build IAHWC"
    print_option_help "--build-weston" "build WESTON"
    print_option_help "--build" "build both"
    print_option_help "--iahwc-dir=" "path to iahwc source"
    print_option_help "--weston-dir=" "path to weston source"
    print_option_help "-j=, --parallel=" "number of threads to be used while building"
    print_option_help "-h, --help" "print this message and exit"
    echo
    gettext "Report issues at https://github.com/intel/IA-Hardware-Composer/issues"; echo
    echo
    exit;
}

function build_iahwc() {
    # Build IAHWC
    pushd . > /dev/null
    cd $IAHWC_DIR
    ./autogen.sh $AUTOGEN_CMDLINE --enable-linux-frontend && \
        make -j$PARALLEL install
    popd
}

function build_weston() {
    # Build weston
    export WESTON_NATIVE_BACKEND=iahwc-backend.so
    ./autogen.sh $AUTOGEN_CMDLINE --enable-iahwc-compositor \
      --disable-wayland-compositor --disable-rdp-compositor \
        --disable-headless-compositor --disable-x11-compositor \
        --disable-fbdev-compositor --disable-drm-compositor \
        --enable-demo-clients-install --enable-clients \
        --disable-setuid-install && make -j$PARALLEL && make install
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

[ $BUILD_IAHWC -eq 1 ] && build_iahwc;
[ $BUILD_WESTON -eq 1 ] && build_weston;
