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

#include "mathematics.h"
// #include "opt.h"
#include "parseutils.h"
#include "pixdesc.h"
// #include "imgutils.h"
#include "avassert.h"
#include "swscale.h"


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

    // // 释放bs_data内存
    // free(bs_data);

    return 0;
}

static int writeAVFrame(const char *output_filename, AVFrame *frame) {

    // 打开文件准备写入数据
    FILE *file = fopen(output_filename, "wb");
    if (file == NULL) {
        printf("Fail to open output file! \n");
        return -1;
    }

    // 计算总数据长度
    uint32_t length = frame->Ysize + 2 * frame->UVsize;

    // 写入 Y 数据
    fwrite(frame->data[0], sizeof(uint8_t), frame->Ysize, file);

    // 写入 U 数据
    fwrite(frame->data[1], sizeof(uint8_t), frame->UVsize, file);

    // 写入 V 数据
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
        frame->linesize[1] = width / 2;
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
    //初始化 之后考虑删掉
    frame->sample_aspect_ratio.num = 0;
    frame->sample_aspect_ratio.den = 1;
    frame->color_range = 2;  //JPEG
    frame->colorspace = 2;   //AVCOL_SPC_UNSPECIFIED
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
    // if (outframe->data[0] = NULL){
    outframe->data[0] = av_mallocz(outframe->Ysize);
    outframe->data[1] = av_mallocz(outframe->Ysize);
    outframe->data[2] = av_mallocz(outframe->Ysize);
    // }
    if(ret = readAVFrame(infilename, inframe) !=0 )
        return -1;

    const AVPixFmtDescriptor *desc = av_pix_fmt_desc_get(infmt);  // 获取format
    struct SwsContext *sws;
    ScaleContext *scale = (ScaleContext *)malloc(sizeof(ScaleContext));  //分配内存

    //初始化scale
    scale->flags = 2;

    scale->in_range = 0;
    scale->out_range = 0;
    scale->out_h_chr_pos = -513;
    scale->out_v_chr_pos = -513;
    scale->in_h_chr_pos  = -513;
    scale->in_v_chr_pos  = -513;
    scale->in_color_matrix = "auto";
    scale->in_range = NULL;
    sws_freeContext(scale->sws);

    scale->sws = NULL;

    // struct SwsContext **swscs =
    int i = 0;

    // for (i = 0; i < 3; i++) {
    int in_v_chr_pos = scale->in_v_chr_pos, out_v_chr_pos = scale->out_v_chr_pos;
    struct SwsContext **s = &scale->sws;
    *s = sws_alloc_context();
    if (!*s)
        return AVERROR(ENOMEM);
    // 赋值操作
    (*s)->srcW = srcW;
    (*s)->srcH = srcH >> !!i;
    (*s)->srcFormat = infmt;
    (*s)->dstW = dstW;
    (*s)->dstH = dstH >> !!i;
    (*s)->dstFormat = outfmt;
    (*s)->flags = scale->flags;
    (*s)->chrDstHSubSample = (*s)->chrDstVSubSample = (*s)->chrSrcHSubSample = (*s)->chrSrcVSubSample = inframe->subsample;
    /* Override YUV420P default settings to have the correct (MPEG-2) chroma positions
    * MPEG-2 chroma positions are used by convention
    * XXX: support other 4:2:0 pixel formats */
    if (infmt == AV_PIX_FMT_YUV420P && in_v_chr_pos == -513) {
        in_v_chr_pos = (i == 0) ? 128 : (i == 1) ? 64 : 192;
    }

    if (outfmt == AV_PIX_FMT_YUV420P && out_v_chr_pos == -513) {
        out_v_chr_pos = (i == 0) ? 128 : (i == 1) ? 64 : 192;
    }

    (*s)->src_h_chr_pos = scale->in_h_chr_pos;
    (*s)->src_v_chr_pos = in_v_chr_pos;
    (*s)->dst_h_chr_pos = scale->out_h_chr_pos;
    (*s)->dst_v_chr_pos = out_v_chr_pos;

    if ((ret = sws_init_context(*s, NULL, NULL)) < 0) // 初始化，这里初始化了filter
        return ret;

    //调用filter_frame来进行实际的缩放操作
    if(ret = filter_frame(scale, inframe, outframe) < 0){
        fprintf(stderr, "Scale failed! \n");
        return ret;
    }

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