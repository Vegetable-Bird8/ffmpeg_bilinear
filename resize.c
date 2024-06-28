/*
This is the part that do the actual scale
 */

#include <inttypes.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "swscale_internal.h"

void sws_freeContext(SwsContext *c)
{
    int i;
    if (!c)
        return;


    av_freep(&c->vLumFilter);
    av_freep(&c->vChrFilter);
    av_freep(&c->hLumFilter);
    av_freep(&c->hChrFilter);

    av_freep(&c->vLumFilterPos);
    av_freep(&c->vChrFilterPos);
    av_freep(&c->hLumFilterPos);
    av_freep(&c->hChrFilterPos);


    ff_free_filters(c);

    av_free(c);
}

int filter_frame(SwsContext *c, AVFrame *in, AVFrame *out)
{
    // const AVPixFmtDescriptor *desc = av_pix_fmt_desc_get(in->format);
    // char buf[32];


    // scale->hsub = desc->log2_chroma_w;
    // scale->vsub = desc->log2_chroma_h;

    int srcSliceY_internal = 0;
    int ret = c->swscale(c, in->data, in->linesize, srcSliceY_internal, in->height, out->data, out->linesize); //真正做缩放的地方
    // sws_scale(sws, in->data, in_stride, in->height, out->data, out_stride);

    return 0;

}
