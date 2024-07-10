/*
* This is a demo for passing data to resize and run
* The input include YUV and srcW 、 srcH 、dstW and dstH
* The process of extracting YUV data from FFmpeg to pass it into `sws_init_context` should be omitted.
*/
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "swscale_internal.h"
#include "pixdesc.h"


static int readAVFrame(const char *image_yuv, AVFrame *frame) {

    FILE *file = fopen(image_yuv, "rb");
    if (file == NULL) {
        printf("Fail to open input YUV!\n");
        return -1;
    }

    fseek(file, 0, SEEK_END);
    uint32_t length = ftell(file);
    fseek(file, 0, SEEK_SET);

    uint8_t *bs_data = malloc(length);
    if (bs_data == NULL) {
        printf("Memory allocation failed!\n");
        fclose(file);
        return -1;
    }

    fread(bs_data, sizeof(uint8_t), length, file);
    frame->data[0] = bs_data;
    frame->data[1] = bs_data + frame->Ysize;
    frame->data[2] = bs_data + frame->Ysize + frame->UVsize;

    fclose(file);

    return 0;
}

static int writeAVFrame(const char *output_filename, AVFrame *frame) {

    // 打开文件准备写入数据
    FILE *file = fopen(output_filename, "wb");
    if (file == NULL) {
        printf("Fail to open output file! \n");
        return -1;
    }

    // 写入 Y 数据
    fwrite(frame->data[0], sizeof(uint8_t), frame->Ysize, file);

    fwrite(frame->data[1], sizeof(uint8_t), frame->UVsize, file);
    if(frame->UVsize != frame->Ysize / 2)  // 说明不是NV12 或者 NV21，UV分开存储
        fwrite(frame->data[2], sizeof(uint8_t), frame->UVsize, file);

    fclose(file);
    return 0;
}


static int initAVFrame(AVFrame *frame, unsigned int width, unsigned int height, enum AVPixelFormat pixelFormat){
    // 分配AVFrame所需的内存

    if (!frame) {
        // 处理内存分配失败的情况
        printf("failed for alloc !\n");
        return -1;
    }
    //不考虑对齐

    frame->linesize[0] = width;
    frame->linesize[3] = 0;
    if (pixelFormat == AV_PIX_FMT_NV21 || pixelFormat == AV_PIX_FMT_NV12 ) {

        frame->linesize[1] = width;
        frame->linesize[2] = 0;
        frame->subsample = 1;
        frame->Ysize = frame->linesize[0] * height;
        frame->UVsize = frame->linesize[1] * height/2;
    } else if( pixelFormat == AV_PIX_FMT_YUV420P ){

        frame->linesize[1] = width / 2 ;
        frame->linesize[2] = width / 2;
        frame->subsample = 1;// 用来做位移
        frame->Ysize = frame->linesize[0] * height;
        frame->UVsize = frame->linesize[1] * height/2;

    }else if (AV_PIX_FMT_YUV444P) {

        frame->linesize[1] = width;
        frame->linesize[2] = width;
        frame->subsample = 0;
        frame->Ysize = frame->UVsize = frame->linesize[0] * height;
    }
    // 设置AVFrame的宽度、高度和像素格式
    frame->width = width;
    frame->height = height;
    frame->format = pixelFormat;
    return 0;
}

enum AVPixelFormat getPixelFormatFromString(const char *format) {
    if (strcmp(format, "YUV420P") == 0) {
        return AV_PIX_FMT_YUV420P;
    } else if (strcmp(format, "YUV444P") == 0) {
        return AV_PIX_FMT_YUV444P;
    } else if (strcmp(format, "NV12") == 0) {
        return AV_PIX_FMT_NV12;
    } else if (strcmp(format, "NV21") == 0) {
        return AV_PIX_FMT_NV21;
    // 其他枚举值的映射
    } else {
        // 处理无效格式的情况
        return AV_PIX_FMT_YUV420P; // 或者其他默认值
    }
}
// 输入应该包括：
// 1 输入YUV路径
// 2 指定输入宽
// 3 指定输入高
// 4 指定输入YUV格式，输出和输入保持一致
// 5 指定输出宽
// 6 指定输出高
int main(int argc, char* argv[])
{

    if (argc != 8)
    {
        fprintf(stderr, "Usage:%s <YUV IN PATH> <srcW> <srcH> <pixformat> <dstW> <dstH> <YUV OUT PATH>\n", argv[0]);
        return -1;
    }
    int ret;
    const char *infilename  = argv[1];
    const char *outfilename = argv[7];
    unsigned int srcW, srcH, dstW, dstH;
    enum AVPixelFormat infmt, outfmt;
    srcW = atoi(argv[2]);
    srcH = atoi(argv[3]);
    infmt = outfmt = getPixelFormatFromString(argv[4]);
    dstW = atoi(argv[5]);
    dstH = atoi(argv[6]);
    AVFrame *inframe, *outframe;
    inframe = av_mallocz(sizeof(AVFrame));
    initAVFrame(inframe, srcW, srcH, infmt);
    outframe = av_mallocz(sizeof(AVFrame));

    initAVFrame(outframe, dstW, dstH, infmt);

    outframe->data[0] = av_mallocz(outframe->Ysize);
    outframe->data[1] = av_mallocz(outframe->Ysize);
    outframe->data[2] = av_mallocz(outframe->Ysize);

    if(ret = readAVFrame(infilename, inframe) !=0 ){
        printf("Read Data Failed!\n");
        return -1;
    }

    struct SwsContext *s = av_mallocz(sizeof(SwsContext));

    if (!s)
        return -12;
    // 赋值操作
    s->srcW = srcW;
    s->srcH = srcH;
    s->srcFormat = infmt;
    s->dstW = dstW;
    s->dstH = dstH;
    s->dstFormat = outfmt;
    s->flags = 2;
    s->chrDstHSubSample = s->chrDstVSubSample = s->chrSrcHSubSample = s->chrSrcVSubSample = inframe->subsample;

    if ((ret = sws_init_context(s)) < 0) // 初始化，这里初始化了filter
        return ret;

    int srcSliceY_internal = 0;
    s->swscale(s, inframe->data, inframe->linesize, srcSliceY_internal, srcH, outframe->data, outframe->linesize); //真正做缩放的地方

    if (ret = writeAVFrame(outfilename,outframe) != 0){
        printf("Data dump failed! \n");
        return -1;
    }else{
        printf("Data dump success! \n");
    }

    free(inframe);
    free(outframe);
    inframe = NULL;
    outframe = NULL;

    return 0;
}