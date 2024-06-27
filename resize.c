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
#include "mathematics.h"
#include "pixdesc.h"

#include "avutil.h"
// #include "bswap.h"
// #include "intreadwrite.h"
#include "config.h"

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

    // av_freep(&c->gamma);
    // av_freep(&c->inv_gamma);

    ff_free_filters(c);

    av_free(c);
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

    const AVPixFmtDescriptor *desc = av_pix_fmt_desc_get(in->format);
    char buf[32];
    int in_range = in->color_range;


    scale->hsub = desc->log2_chroma_w;
    scale->vsub = desc->log2_chroma_h;

    int in_full, out_full, brightness, contrast, saturation;
    int *inv_table, *table;
    sws_getColorspaceDetails(sws, (int **)&inv_table, &in_full,
                                (int **)&table, &out_full,
                                &brightness, &contrast, &saturation);

    inv_table = ff_yuv2rgb_coeffs[2];

    table = inv_table;

    if (scale-> in_range != AVCOL_RANGE_UNSPECIFIED)
        in_full  = (scale-> in_range == AVCOL_RANGE_JPEG);
    else if (in_range != AVCOL_RANGE_UNSPECIFIED)
        in_full  = (in_range == AVCOL_RANGE_JPEG);
    if (scale->out_range != AVCOL_RANGE_UNSPECIFIED)
        out_full = (scale->out_range == AVCOL_RANGE_JPEG);


    sws_setColorspaceDetails(scale->sws, inv_table, in_full,
                                table, out_full,
                                brightness, contrast, saturation);
    int srcSliceY_internal = 0;
    int ret = sws->swscale(sws, in->data, in->linesize, srcSliceY_internal, in->height, out->data, out->linesize); //真正做缩放的地方
    // sws_scale(sws, in->data, in_stride, in->height, out->data, out_stride);

    return 0;

}
