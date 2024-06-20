/*
This is the part that do the actual scale
 */

#include <inttypes.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "swscale.h"
#include "swscale_internal.h"
#include "log.h"
#include "mathematics.h"
// #include "opt.h"
#include "parseutils.h"
#include "pixdesc.h"
// #include "imgutils.h"
#include "avassert.h"

#include "avassert.h"
#include "avutil.h"
#include "pixdesc.h"
#include "bswap.h"
// #include "cpu.h"
// #include "imgutils.h"
#include "intreadwrite.h"
#include "mathematics.h"
#include "pixdesc.h"
#include "config.h"
// #include "rgb2rgb.h"


#define DEBUG_SWSCALE_BUFFERS 0
#define DEBUG_BUFFERS(...)                      \
    if (DEBUG_SWSCALE_BUFFERS)                  \
        av_log(NULL, AV_LOG_DEBUG, __VA_ARGS__)

DECLARE_ALIGNED(8, static const uint8_t, sws_pb_64)[8] = {
    64, 64, 64, 64, 64, 64, 64, 64
};


static void reset_ptr(const uint8_t *src[], enum AVPixelFormat format)
{
    // if (!isALPHA(format))
    src[3] = NULL;
    if (!isPlanar(format)) {
        src[3] = src[2] = NULL;

        if (!usePal(format))
            src[1] = NULL;
    }
}

static int check_image_pointers(const uint8_t * const data[4], enum AVPixelFormat pix_fmt,
                                const int linesizes[4])
{
    const AVPixFmtDescriptor *desc = av_pix_fmt_desc_get(pix_fmt);
    int i;

    // av_assert2(desc);

    for (i = 0; i < 4; i++) {
        int plane = desc->comp[i].plane;
        if (!data[plane] || !linesizes[plane])
            return 0;
    }

    return 1;
}


/**
 * swscale wrapper, so we don't need to export the SwsContext.
 * Assumes planar YUV to be in YUV order instead of YVU.
 */
int sws_scale(struct SwsContext *c,
            const uint8_t * const srcSlice[],
            const int srcStride[], int srcSliceY,
            int srcSliceH, uint8_t *const dst[],
            const int dstStride[])
{
    int i, ret;
    const uint8_t *src2[4];
    uint8_t *dst2[4];
    uint8_t *rgb0_tmp = NULL;
    int macro_height = isBayer(c->srcFormat) ? 2 : (1 << c->chrSrcVSubSample);
    // copy strides, so they can safely be modified
    int srcStride2[4];
    int dstStride2[4];
    int srcSliceY_internal = srcSliceY;

    if (!srcStride || !dstStride || !dst || !srcSlice) {
        printf( "One of the input parameters to sws_scale() is NULL, please check the calling code\n");
        return 0;
    }

    for (i=0; i<4; i++) {
        srcStride2[i] = srcStride[i];
        dstStride2[i] = dstStride[i];
    }

    if ((srcSliceY & (macro_height-1)) ||
        ((srcSliceH& (macro_height-1)) && srcSliceY + srcSliceH != c->srcH) ||
        srcSliceY + srcSliceH > c->srcH) {
        printf( "Slice parameters %d, %d are invalid\n", srcSliceY, srcSliceH);
        return AVERROR(EINVAL);
    }
    //这里不走，删除
    if (c->gamma_flag && c->cascaded_context[0]) {


        ret = sws_scale(c->cascaded_context[0],
                    srcSlice, srcStride, srcSliceY, srcSliceH,
                    c->cascaded_tmp, c->cascaded_tmpStride);

        if (ret < 0)
            return ret;

        if (c->cascaded_context[2])
            ret = sws_scale(c->cascaded_context[1], (const uint8_t * const *)c->cascaded_tmp, c->cascaded_tmpStride, srcSliceY, srcSliceH, c->cascaded1_tmp, c->cascaded1_tmpStride);
        else
            ret = sws_scale(c->cascaded_context[1], (const uint8_t * const *)c->cascaded_tmp, c->cascaded_tmpStride, srcSliceY, srcSliceH, dst, dstStride);

        if (ret < 0)
            return ret;

        if (c->cascaded_context[2]) {
            ret = sws_scale(c->cascaded_context[2],
                        (const uint8_t * const *)c->cascaded1_tmp, c->cascaded1_tmpStride, c->cascaded_context[1]->dstY - ret, c->cascaded_context[1]->dstY,
                        dst, dstStride);
        }
        return ret;
    }
    // 这里也不走 删除
    if (c->cascaded_context[0] && srcSliceY == 0 && srcSliceH == c->cascaded_context[0]->srcH) {
        ret = sws_scale(c->cascaded_context[0],
                        srcSlice, srcStride, srcSliceY, srcSliceH,
                        c->cascaded_tmp, c->cascaded_tmpStride);
        if (ret < 0)
            return ret;
        ret = sws_scale(c->cascaded_context[1],
                        (const uint8_t * const * )c->cascaded_tmp, c->cascaded_tmpStride, 0, c->cascaded_context[0]->dstH,
                        dst, dstStride);
        return ret;
    }
    // 内存拷贝，将输入和输出的数据拷贝到src2和dst2中
    memcpy(src2, srcSlice, sizeof(src2));
    memcpy(dst2, dst, sizeof(dst2));

    // do not mess up sliceDir if we have a "trailing" 0-size slice
    if (srcSliceH == 0)
        return 0;

    if (!check_image_pointers(srcSlice, c->srcFormat, srcStride)) {
        printf( "bad src image pointers\n");
        return 0;
    }
    // if (!check_image_pointers((const uint8_t* const*)dst, c->dstFormat, dstStride)) {
    //     printf( "bad dst image pointers\n");
    //     return 0;
    // }

    if (c->sliceDir == 0 && srcSliceY != 0 && srcSliceY + srcSliceH != c->srcH) {
        printf( "Slices start in the middle!\n");
        return 0;
    }
    if (c->sliceDir == 0) {
        if (srcSliceY == 0) c->sliceDir = 1; else c->sliceDir = -1;
    }

    // if (usePal(c->srcFormat)) {  //用不到
    // }

    // if (c->src0Alpha && !c->dst0Alpha && isALPHA(c->dstFormat)) {   //用不到
    // }

    // if (c->srcXYZ && !(c->dstXYZ && c->srcW==c->dstW && c->srcH==c->dstH)) {  //初始化为0
    // }

    if (!srcSliceY && (c->flags & SWS_BITEXACT) && c->dither == SWS_DITHER_ED && c->dither_error[0])
        for (i = 0; i < 4; i++)
            memset(c->dither_error[i], 0, sizeof(c->dither_error[0][0]) * (c->dstW+2));
    // 下面应该也走不到
    if (c->sliceDir != 1) {
        // slices go from bottom to top => we flip the image internally
        for (i=0; i<4; i++) {
            srcStride2[i] *= -1;
            dstStride2[i] *= -1;
        }

        src2[0] += (srcSliceH - 1) * srcStride[0];
        if (!usePal(c->srcFormat))
            src2[1] += ((srcSliceH >> c->chrSrcVSubSample) - 1) * srcStride[1];
        src2[2] += ((srcSliceH >> c->chrSrcVSubSample) - 1) * srcStride[2];
        src2[3] += (srcSliceH - 1) * srcStride[3];
        dst2[0] += ( c->dstH                         - 1) * dstStride[0];
        dst2[1] += ((c->dstH >> c->chrDstVSubSample) - 1) * dstStride[1];
        dst2[2] += ((c->dstH >> c->chrDstVSubSample) - 1) * dstStride[2];
        dst2[3] += ( c->dstH                         - 1) * dstStride[3];

        srcSliceY_internal = c->srcH-srcSliceY-srcSliceH;
    }
    reset_ptr(src2, c->srcFormat);
    reset_ptr((void*)dst2, c->dstFormat);

    /* reset slice direction at end of frame */
    if (srcSliceY_internal + srcSliceH == c->srcH)
        c->sliceDir = 0;
    ret = c->swscale(c, src2, srcStride2, srcSliceY_internal, srcSliceH, dst2, dstStride2);


    // if (c->dstXYZ && !(c->srcXYZ && c->srcW==c->dstW && c->srcH==c->dstH)) {
    //     int dstY = c->dstY ? c->dstY : srcSliceY + srcSliceH;
    //     uint16_t *dst16 = (uint16_t*)(dst2[0] + (dstY - ret) * dstStride2[0]);
    //     av_assert0(dstY >= ret);
    //     av_assert0(ret >= 0);
    //     av_assert0(c->dstH >= dstY);

    //     /* replace on the same data */
    //     rgb48Toxyz12(c, dst16, dst16, dstStride2[0]/2, ret);
    // }

    av_free(rgb0_tmp);
    return ret;
}

void sws_freeContext(SwsContext *c)
{
    int i;
    if (!c)
        return;

    for (i = 0; i < 4; i++)
        av_freep(&c->dither_error[i]);

    av_freep(&c->vLumFilter);
    av_freep(&c->vChrFilter);
    av_freep(&c->hLumFilter);
    av_freep(&c->hChrFilter);
#if HAVE_ALTIVEC
    av_freep(&c->vYCoeffsBank);
    av_freep(&c->vCCoeffsBank);
#endif

    av_freep(&c->vLumFilterPos);
    av_freep(&c->vChrFilterPos);
    av_freep(&c->hLumFilterPos);
    av_freep(&c->hChrFilterPos);

    av_freep(&c->yuvTable);
    av_freep(&c->formatConvBuffer);

    sws_freeContext(c->cascaded_context[0]);
    sws_freeContext(c->cascaded_context[1]);
    sws_freeContext(c->cascaded_context[2]);
    memset(c->cascaded_context, 0, sizeof(c->cascaded_context));
    av_freep(&c->cascaded_tmp[0]);
    av_freep(&c->cascaded1_tmp[0]);

    av_freep(&c->gamma);
    av_freep(&c->inv_gamma);

    ff_free_filters(c);

    av_free(c);
}

static int scale_slice(AVFrame *out_buf, AVFrame *cur_pic, struct SwsContext *sws, int y, int h, int mul)
{
    // ScaleContext *scale = link->dst->priv;
    const uint8_t *in[4];
    uint8_t *out[4];
    int in_stride[4],out_stride[4];
    int i;

    for(i=0; i<4; i++){
         in_stride[i] = cur_pic->linesize[i];
        out_stride[i] = out_buf->linesize[i];
         in[i] = cur_pic->data[i];
        out[i] = out_buf->data[i];
    }
    //用不到
    // if(scale->input_is_pal)
    //      in[1] = cur_pic->data[1];
    // if(scale->output_is_pal)
    //     out[1] = out_buf->data[1];

    return sws_scale(sws, in, in_stride, y/mul, h,
                         out,out_stride);
}

int sws_getColorspaceDetails(struct SwsContext *c, int **inv_table,
                             int *srcRange, int **table, int *dstRange,
                             int *brightness, int *contrast, int *saturation)
{
    if (!c )
        return -1;

    *inv_table  = c->srcColorspaceTable;
    *table      = c->dstColorspaceTable;
    *srcRange   = c->srcRange;
    *dstRange   = c->dstRange;
    *brightness = c->brightness;
    *contrast   = c->contrast;
    *saturation = c->saturation;

    return 0;
}

int filter_frame(ScaleContext *scale, AVFrame *in, AVFrame *out)
{
    struct SwsContext *sws = scale->sws;
    // AVFilterLink *outlink = link->dst->outputs[0];
    // AVFrame *out;
    // int ret;
    // if(ret = initAVFrame(out,))
    // enum AVPixelFormat infmt = in->format;
    const AVPixFmtDescriptor *desc = av_pix_fmt_desc_get(in->format);
    char buf[32];
    int in_range = in->color_range;


    scale->hsub = desc->log2_chroma_w;
    scale->vsub = desc->log2_chroma_h;

    int in_full, out_full, brightness, contrast, saturation;
    const int *inv_table, *table;
    sws_getColorspaceDetails(sws, (int **)&inv_table, &in_full,
                                (int **)&table, &out_full,
                                &brightness, &contrast, &saturation);

    table = inv_table;

    // if (scale-> in_range != AVCOL_RANGE_UNSPECIFIED)
    //     in_full  = (scale->in_range == AVCOL_RANGE_JPEG);
    // else if (in_range != AVCOL_RANGE_UNSPECIFIED)
    //     in_full  = (in_range == AVCOL_RANGE_JPEG);

    out_full = in_full;

    sws_setColorspaceDetails(scale->sws, inv_table, in_full,
                                table, out_full,
                                brightness, contrast, saturation);
    if (scale->isws[0])
        sws_setColorspaceDetails(scale->isws[0], inv_table, in_full,
                                    table, out_full,
                                    brightness, contrast, saturation);
    if (scale->isws[1])
        sws_setColorspaceDetails(scale->isws[1], inv_table, in_full,
                                    table, out_full,
                                    brightness, contrast, saturation);

    // scale_slice(scale, out, in, sws, 0, in->height, 1, 0);  //最后一个参数用于奇偶数行扫描，用不到
    scale_slice(out, in, sws, 0, in->height, 1);
    // free(&in);
    return 0;
    // return ff_filter_frame(outlink, out);  //不需要处理out
}
