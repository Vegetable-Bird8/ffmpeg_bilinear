/*
 * Copyright (C) 2015 Pedro Arthur <bygrandao@gmail.com>
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */
#include "swscale_internal.h"

typedef struct VScalerContext
{
    uint16_t *filter[2];
    int32_t  *filter_pos;
    int filter_size;
    void *pfn;
    yuv2packedX_fn yuv2packedX;
} VScalerContext;


static int lum_planar_vscale(SwsContext *c, SwsFilterDescriptor *desc, int sliceY, int sliceH)
{
    VScalerContext *inst = desc->instance;
    int dstW = desc->dst->width;

    int first = FFMAX(1-inst->filter_size, inst->filter_pos[sliceY]);
    int sp = first - desc->src->plane[0].sliceY;
    int dp = sliceY - desc->dst->plane[0].sliceY;
    uint8_t **src = desc->src->plane[0].line + sp;
    uint8_t **dst = desc->dst->plane[0].line + dp;
    uint16_t *filter = inst->filter[0] + sliceY * inst->filter_size;

    if (inst->filter_size == 1)
        ((yuv2planar1_fn)inst->pfn)((const int16_t*)src[0], dst[0], dstW, c->lumDither8, 0);
    else
        ((yuv2planarX_fn)inst->pfn)(filter, inst->filter_size, (const int16_t**)src, dst[0], dstW, c->lumDither8, 0);

    return 1;
}

static int chr_planar_vscale(SwsContext *c, SwsFilterDescriptor *desc, int sliceY, int sliceH)
{
    const int chrSkipMask = (1 << desc->dst->v_chr_sub_sample) - 1;
    if (sliceY & chrSkipMask)
        return 0;
    else {
        VScalerContext *inst = desc->instance;
        int dstW = AV_CEIL_RSHIFT(desc->dst->width, desc->dst->h_chr_sub_sample);
        int chrSliceY = sliceY >> desc->dst->v_chr_sub_sample;

        int first = FFMAX(1-inst->filter_size, inst->filter_pos[chrSliceY]);
        int sp1 = first - desc->src->plane[1].sliceY;
        int sp2 = first - desc->src->plane[2].sliceY;
        int dp1 = chrSliceY - desc->dst->plane[1].sliceY;
        int dp2 = chrSliceY - desc->dst->plane[2].sliceY;
        uint8_t **src1 = desc->src->plane[1].line + sp1;
        uint8_t **src2 = desc->src->plane[2].line + sp2;
        uint8_t **dst1 = desc->dst->plane[1].line + dp1;
        uint8_t **dst2 = desc->dst->plane[2].line + dp2;
        uint16_t *filter = inst->filter[0] + chrSliceY * inst->filter_size;

        if (c->yuv2nv12cX) {
            ((yuv2interleavedX_fn)inst->pfn)(c, filter, inst->filter_size, (const int16_t**)src1, (const int16_t**)src2, dst1[0], dstW);
        } else if (inst->filter_size == 1) {
            ((yuv2planar1_fn)inst->pfn)((const int16_t*)src1[0], dst1[0], dstW, c->chrDither8, 0);
            ((yuv2planar1_fn)inst->pfn)((const int16_t*)src2[0], dst2[0], dstW, c->chrDither8, 3);
        } else {
            ((yuv2planarX_fn)inst->pfn)(filter, inst->filter_size, (const int16_t**)src1, dst1[0], dstW, c->chrDither8, 0);
            ((yuv2planarX_fn)inst->pfn)(filter, inst->filter_size, (const int16_t**)src2, dst2[0], dstW, c->chrDither8, 3);
        }
    }

    return 1;
}

int ff_init_vscale(SwsContext *c, SwsFilterDescriptor *desc, SwsSlice *src, SwsSlice *dst)
{
    VScalerContext *lumCtx = NULL;
    VScalerContext *chrCtx = NULL;

    if (isPlanarYUV(c->dstFormat) /*|| (isGray(c->dstFormat) && !isALPHA(c->dstFormat)) */) {
        lumCtx = av_mallocz(sizeof(VScalerContext));
        if (!lumCtx)
            return AVERROR(ENOMEM);


        desc[0].process = lum_planar_vscale;
        desc[0].instance = lumCtx;
        desc[0].src = src;
        desc[0].dst = dst;

        if (!isGray(c->dstFormat)) {
            chrCtx = av_mallocz(sizeof(VScalerContext));
            if (!chrCtx)
                return AVERROR(ENOMEM);
            desc[1].process = chr_planar_vscale;
            desc[1].instance = chrCtx;
            desc[1].src = src;
            desc[1].dst = dst;
        }
    }

    ff_init_vscale_pfn(c, c->yuv2plane1, c->yuv2planeX, c->yuv2nv12cX);
    return 0;
}

void ff_init_vscale_pfn(SwsContext *c,
    yuv2planar1_fn yuv2plane1,
    yuv2planarX_fn yuv2planeX,
    yuv2interleavedX_fn yuv2nv12cX)
{
    VScalerContext *lumCtx = NULL;
    VScalerContext *chrCtx = NULL;
    int idx = c->numDesc - 1;

    if (isPlanarYUV(c->dstFormat) /*|| (isGray(c->dstFormat) && !isALPHA(c->dstFormat))*/) {
        if (!isGray(c->dstFormat)) {
            chrCtx = c->desc[idx].instance;

            chrCtx->filter[0] = c->vChrFilter;
            chrCtx->filter_size = c->vChrFilterSize;
            chrCtx->filter_pos = c->vChrFilterPos;
            // chrCtx->isMMX = 0;

            --idx;
            if (yuv2nv12cX)               chrCtx->pfn = yuv2nv12cX;
            else if (c->vChrFilterSize == 1) chrCtx->pfn = yuv2plane1;
            else                             chrCtx->pfn = yuv2planeX;
        }

        lumCtx = c->desc[idx].instance;

        lumCtx->filter[0] = c->vLumFilter;
        lumCtx->filter[1] = c->vLumFilter;
        lumCtx->filter_size = c->vLumFilterSize;
        lumCtx->filter_pos = c->vLumFilterPos;
        // lumCtx->isMMX = 0;

        if (c->vLumFilterSize == 1) lumCtx->pfn = yuv2plane1;
        else                        lumCtx->pfn = yuv2planeX;

    }
}


