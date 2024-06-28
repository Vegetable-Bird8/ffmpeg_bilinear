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
#include "mem.h"
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
        fprintf(stderr, "Usage:%s <YUV FILE PATH> <srcW> <srcH> <pixformat> <dstW> <dstH> <YUV OUT PATH>\n", argv[0]);
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
    // 过量分配内存，防止溢出
    outframe->data[0] = av_mallocz(outframe->Ysize);
    outframe->data[1] = av_mallocz(outframe->Ysize);
    outframe->data[2] = av_mallocz(outframe->Ysize);

    if(ret = readAVFrame(infilename, inframe) !=0 ){
        printf("Read Data Failed!\n");
        return -1;
    }


    const AVPixFmtDescriptor *desc = av_pix_fmt_desc_get(infmt);  // 获取format
    struct SwsContext *sws;
    // ScaleContext *scale = (ScaleContext *)malloc(sizeof(ScaleContext));  //分配内存

    //初始化scale
    // scale->flags = 2;


    // scale->out_h_chr_pos = -513;
    // scale->out_v_chr_pos = -513;
    // scale->in_h_chr_pos  = -513;
    // scale->in_v_chr_pos  = -513;

    // sws_freeContext(scale->sws);

    // scale->sws = NULL;

    int i = 0;

    int in_v_chr_pos = -513;
    int out_v_chr_pos = -513;
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

    /* Override YUV420P default settings to have the correct (MPEG-2) chroma positions
    * MPEG-2 chroma positions are used by convention
    * XXX: support other 4:2:0 pixel formats */
    if (infmt == AV_PIX_FMT_YUV420P && in_v_chr_pos == -513) {
        in_v_chr_pos = 128;
    }

    if (outfmt == AV_PIX_FMT_YUV420P && out_v_chr_pos == -513) {
        out_v_chr_pos = 128;
    }

    s->src_h_chr_pos = -513;
    s->src_v_chr_pos = in_v_chr_pos;
    s->dst_h_chr_pos = -513;
    s->dst_v_chr_pos = out_v_chr_pos;

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

    av_free(inframe);
    av_free(outframe);
    inframe = NULL;
    outframe = NULL;

    return 0;
}