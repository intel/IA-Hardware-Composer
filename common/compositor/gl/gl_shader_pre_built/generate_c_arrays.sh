#!/bin/bash

if [ $# -eq 0 ]
  then
    echo "Need PCI-ID of the target"
    exit
fi

TEMP_LD_LIBRARY_PATH=$LD_LIBRARY_PATH
unset LD_LIBRARY_PATH

SHADER_PRE_BUILT_PATH=$PWD/compositor/gl/gl_shader_pre_built

TARGET_I965=$WLD/lib/dri
TARGET_PCI_ID=$1

OUT_DIR=$SHADER_PRE_BUILT_PATH/shader_prog_arrays
OUT_HEADER=$SHADER_PRE_BUILT_PATH/glprebuiltshaderarray.h

SHADER_TEST_FILE_PREFIX=hwc_shader_prog_
SHADER_TEST_FOLDER=$SHADER_PRE_BUILT_PATH/shader-test
BIN_TO_ARRAY=$SHADER_PRE_BUILT_PATH/bin_to_c_array
SHADER_DB_DIR=$SHADER_PRE_BUILT_PATH/shader-db

MAX_NUM_LAYERS=16
GEN_SHADER_TEST=$SHADER_PRE_BUILT_PATH/generate_shader_test.sh

#building bin_to_c_arrays
gcc $BIN_TO_ARRAY.c -o $BIN_TO_ARRAY

i=1

#generate shader-tests
while [ $i -le $MAX_NUM_LAYERS ]
do
   $GEN_SHADER_TEST $i
   i=$(( $i + 1 ))
done

mkdir -p $SHADER_TEST_FOLDER
mv *.shader_test $SHADER_TEST_FOLDER

#pre-build shader-tests to generate shader program binary files
export LIBGL_DRIVERS_PATH=$TARGET_I965
export LD_PRELOAD=$SHADER_DB_DIR/intel_stub.so${LD_PRELOAD:+:${LD_PRELOAD}}
$SHADER_DB_DIR/run -p $TARGET_PCI_ID -b $SHADER_TEST_FOLDER
unset LIBGL_DRIVERS_PATH
unset LD_PRELOAD
mkdir -p $OUT_DIR

#generate arrays of shader program binary

i=1
while [ $i -le $MAX_NUM_LAYERS ]
do
   $BIN_TO_ARRAY $SHADER_TEST_FOLDER/$SHADER_TEST_FILE_PREFIX$i.shader_test.bin $OUT_DIR/$SHADER_TEST_FILE_PREFIX$i.h
   prebuilt_shader_header+="static uint8_t shader_prog_$i[] = {\n"
   prebuilt_shader_header+="#include "'"'"./gl_shader_pre_built/shader_prog_arrays/$SHADER_TEST_FILE_PREFIX$i.h"'"'"\n"
   prebuilt_shader_header+="};\n\n"
   i=$(( $i + 1 ))
done

prebuilt_shader_header+="static uint8_t *shader_prog_arrays[] = {\n"
i=1
while [ $i -le $MAX_NUM_LAYERS ]
do
   prebuilt_shader_header+="shader_prog_$i,\n"
   i=$(( $i + 1 ))
done
prebuilt_shader_header+="};\n"

echo -e $prebuilt_shader_header > $OUT_HEADER
mv $OUT_HEADER $SHADER_PRE_BUILT_PATH/../

export LD_LIBRARY_PATH=$TEMP_LD_LIBRARY_PATH

echo "ALL DONE"
