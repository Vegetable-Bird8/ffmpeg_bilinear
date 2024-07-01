#!/bin/bash

# ANSI color codes
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m' # No Color

# 运行第一个命令
./test_resize "/home/hsn/middleware/yuv_pic/yuv444/JPEG_1920x1088_yuv444_planar.yuv" 1920 1088 YUV444P 2560 1472 ./tmp_yuv_444_bigger.yuv
md5sum_output1=$(md5sum ./tmp_yuv_444_bigger.yuv | awk '{print $1}')
expected_md5_1="607446854033e6d4a77bfbbb10572d27"

# 运行第二个命令
./test_resize "/home/hsn/middleware/yuv_pic/yuv444/JPEG_1920x1088_yuv444_planar.yuv" 1920 1088 YUV444P 1088 720 ./tmp_yuv_444_smaller.yuv
md5sum_output2=$(md5sum ./tmp_yuv_444_smaller.yuv | awk '{print $1}')
expected_md5_2="9db6dc94dd41c8d8a4d6701742132785"

# 运行第三个命令
./test_resize "/home/hsn/middleware/yuv_pic/yuv420/1088test1_420.yuv" 1920 1088 YUV420P 2560 1472 ./tmp_yuv_420_bigger.yuv
md5sum_output3=$(md5sum ./tmp_yuv_420_bigger.yuv | awk '{print $1}')
expected_md5_3="634d26ba3314ef517ff1f45e11d9bd99"

# 运行第四个命令
./test_resize "/home/hsn/middleware/yuv_pic/yuv420/1088test1_420.yuv" 1920 1088 YUV420P 1088 720 ./tmp_yuv_420_smaller.yuv
md5sum_output4=$(md5sum ./tmp_yuv_420_smaller.yuv | awk '{print $1}')
expected_md5_4="f99b1098136132b345a61f754ed5d254"

# 运行第五个命令 该条和ffmpeg md5值一致
./test_resize "/home/hsn/middleware/yuv_pic/nv21/JPEG_1920x1088_yuv420_nv21.yuv" 1920 1088 NV21 2560 1472 ./tmp_nv21_new.yuv
md5sum_output5=$(md5sum ./tmp_nv21_new.yuv | awk '{print $1}')
expected_md5_5="99682d514fa65e7d8fa67a92f8dcb14e"

# 比较实际MD5值和期望的MD5值
if [ "$md5sum_output1" == "$expected_md5_1" ]; then
    echo -e "${GREEN}第一个命令输出文件的MD5值与期望值匹配${NC}"
else
    echo -e "${RED}第一个命令输出文件的MD5值与期望值不匹配${NC}"
    echo $md5sum_output1
fi

if [ "$md5sum_output2" == "$expected_md5_2" ]; then
    echo -e "${GREEN}第二个命令输出文件的MD5值与期望值匹配${NC}"
else
    echo -e "${RED}第二个命令输出文件的MD5值与期望值不匹配${NC}"
    echo $md5sum_output2
fi

if [ "$md5sum_output3" == "$expected_md5_3" ]; then
    echo -e "${GREEN}第三个命令输出文件的MD5值与期望值匹配${NC}"
else
    echo -e "${RED}第三个命令输出文件的MD5值与期望值不匹配${NC}"
    echo $md5sum_output3
fi

if [ "$md5sum_output4" == "$expected_md5_4" ]; then
    echo -e "${GREEN}第四个命令输出文件的MD5值与期望值匹配${NC}"
else
    echo -e "${RED}第四个命令输出文件的MD5值与期望值不匹配${NC}"
    echo $md5sum_output4
fi

if [ "$md5sum_output5" == "$expected_md5_5" ]; then
    echo -e "${GREEN}第五个命令输出文件的MD5值与期望值匹配${NC}"
else
    echo -e "${RED}第五个命令输出文件的MD5值与期望值不匹配${NC}"
    echo $md5sum_output5
fi