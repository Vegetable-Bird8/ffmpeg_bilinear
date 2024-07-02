#!/bin/bash

# ANSI color codes
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m' # No Color

# 函数定义 - 检查MD5值是否匹配
check_md5_match() {
    md5sum_output=$1
    expected_md5=$2

    if [ "$md5sum_output" == "$expected_md5" ]; then
        echo -e "${GREEN}输出文件的MD5值与期望值匹配${NC}"
    else
        echo -e "${RED}输出文件的MD5值与期望值不匹配${NC}"
        echo $md5sum_output
    fi
}

# 函数定义 - 运行命令并检查MD5值
run_and_check_command() {
    input_file=$1
    width=$2
    height=$3
    format=$4
    new_width=$5
    new_height=$6
    output_file=$7
    expected_md5=$8

    ./test_resize $input_file $width $height $format $new_width $new_height $output_file
    md5sum_output=$(md5sum $output_file | awk '{print $1}')
    check_md5_match $md5sum_output $expected_md5
}

# 运行并检查所有命令
run_and_check_command "/home/hsn/middleware/yuv_pic/yuv444/JPEG_1920x1088_yuv444_planar.yuv" 1920 1088 YUV444P 2560 1472 ./tmp_yuv_444_bigger.yuv "607446854033e6d4a77bfbbb10572d27"
run_and_check_command "/home/hsn/middleware/yuv_pic/yuv444/JPEG_1920x1088_yuv444_planar.yuv" 1920 1088 YUV444P 1088 720 ./tmp_yuv_444_smaller.yuv "9db6dc94dd41c8d8a4d6701742132785"
run_and_check_command "/home/hsn/middleware/yuv_pic/yuv420/1088test1_420.yuv" 1920 1088 YUV420P 2560 1472 ./tmp_yuv_420_bigger.yuv "634d26ba3314ef517ff1f45e11d9bd99"
run_and_check_command "/home/hsn/middleware/yuv_pic/yuv420/1088test1_420.yuv" 1920 1088 YUV420P 1088 720 ./tmp_yuv_420_smaller.yuv "f99b1098136132b345a61f754ed5d254"
run_and_check_command "/home/hsn/middleware/yuv_pic/nv21/JPEG_1920x1088_yuv420_nv21.yuv" 1920 1088 NV21 2560 1472 ./tmp_nv21_new.yuv "99682d514fa65e7d8fa67a92f8dcb14e"
run_and_check_command "/home/hsn/middleware/yuv_pic/yuv444/JPEG_1920x1088_yuv444_planar.yuv" 1920 1088 YUV444P 100 100 ./tmp_100_444.yuv "f2694241f44cca50d0b44085bf6a3bef"
run_and_check_command "/home/hsn/middleware/yuv_pic/nv21/JPEG_1920x1088_yuv420_nv21.yuv" 1920 1088 NV21 24 46 ./tmp_extra_nv21_small.yuv "e81a9c085377017bbdbfccf2f67f0f2d"
run_and_check_command "/home/hsn/middleware/yuv_pic/yuv420/100test4_420.yuv" 100 100 YUV420P 3000 3000 ./tmp_3000_420_bigger.yuv "f6c48180ba669e56793ac16fd6693136"
run_and_check_command "/home/hsn/middleware/yuv_pic/yuv420/200test3_420.yuv" 200 200 YUV420P 8342 5480 ./tmp_extra_420_bigger.yuv "b7dc8936aec39a88bf30eb3d9608987d"
