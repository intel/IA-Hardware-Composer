#!/bin/sh
#
# Copyright (c) 2016 Intel Corporation
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

commands_script_dir=$( cd -P -- "$(dirname -- "$(command -v -- "$0")")" && pwd -P )
cd $commands_script_dir

rm -rf ./resource.*

ffmpeg -i ./test.jpg -s 600x400 -vf scale=600:400 -pix_fmt yuv420p ./resource.yuv420p.yuv
mv ./resource.yuv420p.yuv ./resource.yuv420p
ffmpeg -i ./test.jpg -s 600x400 -vf scale=600:400 -pix_fmt yuv420p16le ./resource.yuv420p16le.yuv
mv ./resource.yuv420p16le.yuv ./resource.yuv420p16le
ffmpeg -i ./test.jpg -s 600x400 -vf scale=600:400 -pix_fmt nv12 ./resource.nv12.yuv
mv ./resource.nv12.yuv ./resource.nv12
# Use yuv422p to replace nv16 because of ffmpeg nv16 bug
# ffmpeg -i ./test.jpg -s 600x400 -vf scale=600:400 -pix_fmt nv16 ./resource.nv16.yuv
# mv ./resource.nv16.yuv ./resource.nv16
ffmpeg -i ./test.jpg -s 600x400 -vf scale=600:400 -pix_fmt nv21 ./resource.nv21.yuv
mv ./resource.nv21.yuv ./resource.nv21
ffmpeg -i ./test.jpg -s 600x400 -vf scale=600:400 -pix_fmt yuv444p ./resource.yuv444p.yuv
mv ./resource.yuv444p.yuv ./resource.yuv444p
ffmpeg -i ./test.jpg -s 600x400 -vf scale=600:400 -pix_fmt yuyv422 ./resource.yuyv422.yuv
mv ./resource.yuyv422.yuv ./resource.yuyv422
ffmpeg -i ./test.jpg -s 600x400 -vf scale=600:400 -pix_fmt yuv422p ./resource.yuv422p.yuv
mv ./resource.yuv422p.yuv ./resource.yuv422p

../colorformatter --input ./resource.yuv420p --input-format yuv420p --width 600 --height 400 --output ./resource.yv12 --output-format yv12
../colorformatter --input ./resource.yuv420p --input-format yuv420p --width 600 --height 400 --output ./resource.y8 --output-format y8
../colorformatter --input ./resource.yuv420p16le --input-format yuv420p16le --width 600 --height 400 --output ./resource.y16 --output-format y16
../colorformatter --input ./resource.nv12 --input-format nv12 --width 600 --height 400 --output ./resource.ycbcr_420_888 --output-format ycbcr_420_888
../colorformatter --input ./resource.nv21 --input-format nv21 --width 600 --height 400 --output ./resource.ycrcb_420_sp --output-format ycrcb_420_sp
#below command line need check if ycbcr_444_888 is plane format?
../colorformatter --input ./resource.yuv444p --input-format yuv444p --width 600 --height 400 --output ./resource.ycbcr_444_888 --output-format ycbcr_444_888
../colorformatter --input ./resource.nv12 --input-format nv12 --width 600 --height 400 --output ./resource.nv12_y_tiled_intel --output-format nv12_y_tiled_intel
../colorformatter --input ./resource.nv12 --input-format nv12 --width 600 --height 400 --output ./resource.nv12_linear_cam_intel --output-format nv12_linear_cam_intel
../colorformatter --input ./resource.yuyv422 --input-format yuyv422 --width 600 --height 400 --output ./resource.ycbcr_422_i --output-format ycbcr_422_i
#below command line need check if ycbcr_422_888 is plane format?
../colorformatter --input ./resource.yuv422p --input-format yuv422p --width 600 --height 400 --output ./resource.ycbcr_422_888 --output-format ycbcr_422_888
../colorformatter --input ./resource.yuv422p --input-format yuv422p --width 600 --height 400 --output ./resource.ycbcr_422_sp --output-format ycbcr_422_sp
../colorformatter --width 600 --height 400 --output ./resource.raw10 --output-format raw10
../colorformatter --width 600 --height 400 --output ./resource.raw12 --output-format raw12
../colorformatter --width 600 --height 400 --output ./resource.raw16 --output-format raw16
../colorformatter --width 600 --height 400 --output ./resource.rawblob --output-format rawblob
../colorformatter --width 600 --height 400 --output ./resource.rawopaque --output-format rawopaque
cd -
