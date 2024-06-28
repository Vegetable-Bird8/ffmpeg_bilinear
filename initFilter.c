#include "limits.h"
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "mem.h"
#include "swscale_internal.h"

DECLARE_ALIGNED(8, static const uint8_t, sws_pb_64)[8] = {
    64, 64, 64, 64, 64, 64, 64, 64
};

static int get_local_pos(SwsContext *s, int chr_subsample, int pos, int dir)
{
    if (pos == -1 || pos <= -513) {
        pos = (128 << chr_subsample) - 128;
    }
    pos += 128; // relative to ideal left edge
    return pos >> chr_subsample;
}

// 计算位置矩阵
static int initFilter(int16_t **outFilter, int32_t **filterPos,
                              int *outFilterSize, int xInc, int srcW,
                              int dstW, int filterAlign, int one,
                              int flags,
                              int srcPos, int dstPos)
{
    int i;
    int filterSize;
    int filter2Size;
    int minFilterSize;
    int64_t *filter    = NULL;
    int64_t *filter2   = NULL;
    const int64_t fone = 1LL << (54 - FFMIN(av_log2(srcW/dstW), 8));
    int ret            = -1;

    *filterPos = av_malloc_array((dstW + 3), sizeof(**filterPos));

    int64_t xDstInSrc;  // 存储用来计算的位置矩阵
    int sizeFactor = 2; //bilinear的sizeFactor=2

    if (xInc <= 1 << 16)    //xInc = srcW/dstW << 16 + 0.5 若小于1<<16 说明是上采样（src<dst)
        filterSize = 1 + sizeFactor;    // upscale
    else
        filterSize = 1 + (sizeFactor * srcW + dstW - 1) / dstW;

    filterSize = FFMIN(filterSize, srcW - 2);
    filterSize = FFMAX(filterSize, 1);

    filter = av_malloc_array(dstW, sizeof(*filter) * filterSize);

    xDstInSrc = ((dstPos*(int64_t)xInc)>>7) - ((srcPos*0x10000LL)>>7);
    for (i = 0; i < dstW; i++) {
        int xx = (xDstInSrc - (filterSize - 2) * (1LL<<16)) / (1 << 17);
        int j;
        (*filterPos)[i] = xx;
        for (j = 0; j < filterSize; j++) {  //仅当j=0时能计算出有小的coeff值
            int64_t d = (FFABS(((int64_t)xx * (1 << 17)) - xDstInSrc)) << 13;
            double floatd;
            int64_t coeff;

            if (xInc > 1 << 16)  // 如果是下采样的话
                d = d * dstW / srcW;
            floatd = d * (1.0 / (1 << 30));  // 计算和最邻近像素位置的距离

            //双线性插值
            coeff = (1 << 30) - d;
            if (coeff < 0)
                coeff = 0;
            coeff *= fone >> 30;  //fone跟缩放系数有关

            filter[i * filterSize + j] = coeff;
            xx++;
        }
        xDstInSrc += 2 * xInc;
    }

    if(filterSize <= 0){
        printf("fileterSize illegal!!\n");
        return -1;
    }

    filter2Size = filterSize;

    if(filter2Size <= 0){
        printf("fileterSize illegal!!\n");
        return -1;
    }
    // FF_ALLOCZ_ARRAY_OR_GOTO(NULL, filter2, dstW, filter2Size * sizeof(*filter2), fail);  //分配和filter一样的内存空间，dstW*filterSize
    filter2 = av_malloc_array(dstW, sizeof(*filter2) * filter2Size);
    for (i = 0; i < dstW; i++) {
        int j, k;

        for (j = 0; j < filterSize; j++)
            filter2[i * filter2Size + j] = filter[i * filterSize + j];

        (*filterPos)[i] += (filterSize - 1) / 2 - (filter2Size - 1) / 2;
    }
    av_freep(&filter);

    /* try to reduce the filter-size (step1 find size and shift left) */
    // Assume it is near normalized (*0.5 or *2.0 is OK but * 0.001 is not).
    minFilterSize = 0;
    for (i = dstW - 1; i >= 0; i--) {
        int min = filter2Size;
        int j;
        int64_t cutOff = 0.0;

        /* get rid of near zero elements on the left by shifting left */
        for (j = 0; j < filter2Size; j++) {
            int k;
            cutOff += FFABS(filter2[i * filter2Size]);

            if (cutOff > SWS_MAX_REDUCE_CUTOFF * fone)
                break;

            /* preserve monotonicity because the core can't handle the
             * filter otherwise */
            if (i < dstW - 1 && (*filterPos)[i] >= (*filterPos)[i + 1])
                break;

            // move filter coefficients left
            for (k = 1; k < filter2Size; k++)
                filter2[i * filter2Size + k - 1] = filter2[i * filter2Size + k];
            filter2[i * filter2Size + k - 1] = 0;
            (*filterPos)[i]++;
        }

        cutOff = 0;
        /* count near zeros on the right */
        for (j = filter2Size - 1; j > 0; j--) {
            cutOff += FFABS(filter2[i * filter2Size + j]);

            if (cutOff > SWS_MAX_REDUCE_CUTOFF * fone)
                break;
            min--;
        }

        if (min > minFilterSize)
            minFilterSize = min;
    }

    // av_assert0(minFilterSize > 0);
    filterSize = (minFilterSize + (filterAlign - 1)) & (~(filterAlign - 1));
    // av_assert0(filterSize > 0);
    filter = av_malloc_array(dstW, filterSize * sizeof(*filter));
    if (!filter)
        goto fail;
    if (filterSize >= MAX_FILTER_SIZE * 16 /
                      ((flags & SWS_ACCURATE_RND) ? 24 : 16)) {
        ret = RETCODE_USE_CASCADE;
        goto fail;
    }
    *outFilterSize = filterSize;

    /* try to reduce the filter-size (step2 reduce it) */
    for (i = 0; i < dstW; i++) {
        int j;

        for (j = 0; j < filterSize; j++) {
            if (j >= filter2Size)
                filter[i * filterSize + j] = 0;
            else
                filter[i * filterSize + j] = filter2[i * filter2Size + j];
            if ((flags & SWS_BITEXACT) && j >= minFilterSize)
                filter[i * filterSize + j] = 0;
        }
    }

    for (i = 0; i < dstW; i++) {
        int j;
        if ((*filterPos)[i] < 0) {
            // move filter coefficients left to compensate for filterPos
            for (j = 1; j < filterSize; j++) {
                int left = FFMAX(j + (*filterPos)[i], 0);
                filter[i * filterSize + left] += filter[i * filterSize + j];
                filter[i * filterSize + j]     = 0;
            }
            (*filterPos)[i]= 0;
        }

        if ((*filterPos)[i] + filterSize > srcW) {
            int shift = (*filterPos)[i] + FFMIN(filterSize - srcW, 0);
            int64_t acc = 0;

            for (j = filterSize - 1; j >= 0; j--) {
                if ((*filterPos)[i] + j >= srcW) {
                    acc += filter[i * filterSize + j];
                    filter[i * filterSize + j] = 0;
                }
            }
            for (j = filterSize - 1; j >= 0; j--) {
                if (j < shift) {
                    filter[i * filterSize + j] = 0;
                } else {
                    filter[i * filterSize + j] = filter[i * filterSize + j - shift];
                }
            }

            (*filterPos)[i]-= shift;
            filter[i * filterSize + srcW - 1 - (*filterPos)[i]] += acc;
        }
    }


    *outFilter = av_malloc_array(dstW + 3, *outFilterSize * sizeof(int16_t));
    /* normalize & store in outFilter */
    for (i = 0; i < dstW; i++) {
        int j;
        int64_t error = 0;
        int64_t sum   = 0;

        for (j = 0; j < filterSize; j++) {
            sum += filter[i * filterSize + j];
        }
        sum = (sum + one / 2) / one;
        if (!sum) {
            printf("SwScaler: zero vector in scaling\n");
            sum = 1;
        }
        for (j = 0; j < *outFilterSize; j++) {
            int64_t v = filter[i * filterSize + j] + error;
            int intV  = ROUNDED_DIV(v, sum);
            (*outFilter)[i * (*outFilterSize) + j] = intV;
            error                                  = v - intV * sum;
        }
    }

    (*filterPos)[dstW + 0] =
    (*filterPos)[dstW + 1] =
    (*filterPos)[dstW + 2] = (*filterPos)[dstW - 1]; /* the MMX/SSE scaler will
                                                      * read over the end */
    for (i = 0; i < *outFilterSize; i++) {
        int k = (dstW - 1) * (*outFilterSize) + i;
        (*outFilter)[k + 1 * (*outFilterSize)] =
        (*outFilter)[k + 2 * (*outFilterSize)] =
        (*outFilter)[k + 3 * (*outFilterSize)] = (*outFilter)[k];
    }

    ret = 0;

fail:
    if(ret < 0)
        printf("sws: initFilter failed\n");
    av_free(filter);
    av_free(filter2);
    return ret;
}

// bilinear / bicubic scaling
// c在这里没起到作用
static void hScale8To15_c(SwsContext *c, int16_t *dst, int dstW,
                          const uint8_t *src, const int16_t *filter,
                          const int32_t *filterPos, int filterSize)
{
    int i;
    for (i = 0; i < dstW; i++) {
        int j;
        int srcPos = filterPos[i];
        int val    = 0;
        for (j = 0; j < filterSize; j++) {
            val += ((int)src[srcPos + j]) * filter[filterSize * i + j];
        }
        dst[i] = FFMIN(val >> 7, (1 << 15) - 1); // the cubic equation does overflow ...
    }
}

static void sws_init_swscale(SwsContext *c)
{
    enum AVPixelFormat srcFormat = c->srcFormat;

    ff_sws_init_output_funcs(c, &c->yuv2plane1, &c->yuv2planeX, &c->yuv2nv12cX);

    ff_sws_init_input_funcs(c);


    c->hyScale = c->hcScale = hScale8To15_c;

    c->needs_hcscale = 1;
}

static int swscale(SwsContext *c, const uint8_t *src[],
                   int srcStride[], int srcSliceY,
                   int srcSliceH, uint8_t *dst[], int dstStride[])
{
    /* load a few things into local vars to make the code more readable?
     * and faster */
    const int dstW                   = c->dstW;
    const int dstH                   = c->dstH;

    const enum AVPixelFormat dstFormat = c->dstFormat;
    const int flags                  = c->flags;
    int32_t *vLumFilterPos           = c->vLumFilterPos;
    int32_t *vChrFilterPos           = c->vChrFilterPos;

    const int vLumFilterSize         = c->vLumFilterSize;
    const int vChrFilterSize         = c->vChrFilterSize;

    yuv2planar1_fn yuv2plane1        = c->yuv2plane1;
    yuv2planarX_fn yuv2planeX        = c->yuv2planeX;
    yuv2interleavedX_fn yuv2nv12cX   = c->yuv2nv12cX;
    const int chrSrcSliceY           =                srcSliceY >> c->chrSrcVSubSample;
    const int chrSrcSliceH           = AV_CEIL_RSHIFT(srcSliceH,   c->chrSrcVSubSample);
    int lastDstY;

    /* vars which will change and which we need to store back in the context */
    int dstY         = c->dstY;
    int lumBufIndex  = c->lumBufIndex;
    int chrBufIndex  = c->chrBufIndex;
    int lastInLumBuf = c->lastInLumBuf;
    int lastInChrBuf = c->lastInChrBuf;


    int lumStart = 0;
    int lumEnd = c->descIndex[0];
    int chrStart = lumEnd;
    int chrEnd = c->descIndex[1];
    int vStart = chrEnd;
    int vEnd = c->numDesc;
    //切片数据被存在了c->slice中
    SwsSlice *src_slice = &c->slice[lumStart];    // 这其实就是c->desc[0]->src
    SwsSlice *hout_slice = &c->slice[c->numSlice-2];  // 这其实就是c->desc[1]->dst 也是c->desc[2]->src
    SwsSlice *vout_slice = &c->slice[c->numSlice-1];   // c->desc[2]->dst
    SwsFilterDescriptor *desc = c->desc;

    int hasLumHoles = 1;
    int hasChrHoles = 1;

    srcStride[1] <<= c->vChrDrop;
    srcStride[2] <<= c->vChrDrop;

    /* Note the user might start scaling the picture in the middle so this
     * will not get executed. This is not really intended but works
     * currently, so people might do it. */
    if (srcSliceY == 0) {
        lumBufIndex  = -1;
        chrBufIndex  = -1;
        dstY         = 0;
        lastInLumBuf = -1;
        lastInChrBuf = -1;
    }

    c->chrDither8 = c->lumDither8 = sws_pb_64;
    lastDstY = dstY;

    ff_init_vscale_pfn(c, yuv2plane1, yuv2planeX, yuv2nv12cX);

    ff_init_slice_from_src(src_slice, (uint8_t**)src, srcStride, c->srcW,
            srcSliceY, srcSliceH, chrSrcSliceY, chrSrcSliceH, 1);
    // 这里只初始化了最终的输出 vout_slice
    ff_init_slice_from_src(vout_slice, (uint8_t**)dst, dstStride, c->dstW,
            dstY, dstH, dstY >> c->chrDstVSubSample,
            AV_CEIL_RSHIFT(dstH, c->chrDstVSubSample), 0);
    if (srcSliceY == 0) {
        hout_slice->plane[0].sliceY = lastInLumBuf + 1;
        hout_slice->plane[1].sliceY = lastInChrBuf + 1;
        hout_slice->plane[2].sliceY = lastInChrBuf + 1;
        hout_slice->plane[3].sliceY = lastInLumBuf + 1;

        hout_slice->plane[0].sliceH =
        hout_slice->plane[1].sliceH =
        hout_slice->plane[2].sliceH =
        hout_slice->plane[3].sliceH = 0;
        hout_slice->width = dstW;
    }
    // 这段代码的主要作用是根据目标图像的行数，计算源图像中需要用作输入的行的位置，处理可能存在的空洞，并更新切片中亮度和色度平面的起始位置和高度信息。
    for (; dstY < dstH; dstY++) {
        const int chrDstY = dstY >> c->chrDstVSubSample;

        // First line needed as input
        const int firstLumSrcY  = FFMAX(1 - vLumFilterSize, vLumFilterPos[dstY]);
        const int firstLumSrcY2 = FFMAX(1 - vLumFilterSize, vLumFilterPos[FFMIN(dstY | ((1 << c->chrDstVSubSample) - 1), dstH - 1)]);
        // First line needed as input
        const int firstChrSrcY  = FFMAX(1 - vChrFilterSize, vChrFilterPos[chrDstY]);

        // Last line needed as input
        int lastLumSrcY  = FFMIN(c->srcH,    firstLumSrcY  + vLumFilterSize) - 1;
        int lastLumSrcY2 = FFMIN(c->srcH,    firstLumSrcY2 + vLumFilterSize) - 1;
        int lastChrSrcY  = FFMIN(c->chrSrcH, firstChrSrcY  + vChrFilterSize) - 1;
        int enough_lines;

        int i;
        int posY, cPosY, firstPosY, lastPosY, firstCPosY, lastCPosY;

        // handle holes (FAST_BILINEAR & weird filters)
        if (firstLumSrcY > lastInLumBuf) {

            hasLumHoles = lastInLumBuf != firstLumSrcY - 1;
            if (hasLumHoles) {
                hout_slice->plane[0].sliceY = firstLumSrcY;
                hout_slice->plane[3].sliceY = firstLumSrcY;
                hout_slice->plane[0].sliceH =
                hout_slice->plane[3].sliceH = 0;
            }

            lastInLumBuf = firstLumSrcY - 1;
        }
        if (firstChrSrcY > lastInChrBuf) {

            hasChrHoles = lastInChrBuf != firstChrSrcY - 1;
            if (hasChrHoles) {
                hout_slice->plane[1].sliceY = firstChrSrcY;
                hout_slice->plane[2].sliceY = firstChrSrcY;
                hout_slice->plane[1].sliceH =
                hout_slice->plane[2].sliceH = 0;
            }

            lastInChrBuf = firstChrSrcY - 1;
        }

        // Do we have enough lines in this slice to output the dstY line
        enough_lines = lastLumSrcY2 < srcSliceY + srcSliceH &&
                       lastChrSrcY < AV_CEIL_RSHIFT(srcSliceY + srcSliceH, c->chrSrcVSubSample);

        if (!enough_lines) {
            lastLumSrcY = srcSliceY + srcSliceH - 1;
            lastChrSrcY = chrSrcSliceY + chrSrcSliceH - 1;
            printf("buffering slice: lastLumSrcY %d lastChrSrcY %d\n",
                          lastLumSrcY, lastChrSrcY);
        }

        // av_assert0((lastLumSrcY - firstLumSrcY + 1) <= hout_slice->plane[0].available_lines);
        // av_assert0((lastChrSrcY - firstChrSrcY + 1) <= hout_slice->plane[1].available_lines);


        posY = hout_slice->plane[0].sliceY + hout_slice->plane[0].sliceH;
        if (posY <= lastLumSrcY && !hasLumHoles) {
            firstPosY = FFMAX(firstLumSrcY, posY);
            lastPosY = FFMIN(firstLumSrcY + hout_slice->plane[0].available_lines - 1, srcSliceY + srcSliceH - 1);
        } else {
            firstPosY = posY;
            lastPosY = lastLumSrcY;
        }

        cPosY = hout_slice->plane[1].sliceY + hout_slice->plane[1].sliceH;
        if (cPosY <= lastChrSrcY && !hasChrHoles) {
            firstCPosY = FFMAX(firstChrSrcY, cPosY);
            lastCPosY = FFMIN(firstChrSrcY + hout_slice->plane[1].available_lines - 1, AV_CEIL_RSHIFT(srcSliceY + srcSliceH, c->chrSrcVSubSample) - 1);
        } else {
            firstCPosY = cPosY;
            lastCPosY = lastChrSrcY;
        }

        ff_rotate_slice(hout_slice, lastPosY, lastCPosY);  // 旋转切片 保证起始高和宽完整

        if (posY < lastLumSrcY + 1) {
            for (i = lumStart; i < lumEnd; ++i)
                desc[i].process(c, &desc[i], firstPosY, lastPosY - firstPosY + 1);
        }

        lumBufIndex += lastLumSrcY - lastInLumBuf;
        lastInLumBuf = lastLumSrcY;

        if (cPosY < lastChrSrcY + 1) {
            for (i = chrStart; i < chrEnd; ++i)
                desc[i].process(c, &desc[i], firstCPosY, lastCPosY - firstCPosY + 1);
        }

        chrBufIndex += lastChrSrcY - lastInChrBuf;
        lastInChrBuf = lastChrSrcY;

        // wrap buf index around to stay inside the ring buffer
        if (lumBufIndex >= vLumFilterSize)
            lumBufIndex -= vLumFilterSize;
        if (chrBufIndex >= vChrFilterSize)
            chrBufIndex -= vChrFilterSize;
        if (!enough_lines)
            break;  // we can't output a dstY line so let's try with the next slice

        if (dstY >= dstH - 2) {
            /* hmm looks like we can't use MMX here without overwriting
             * this array's tail */
            ff_sws_init_output_funcs(c, &yuv2plane1, &yuv2planeX, &yuv2nv12cX);

            ff_init_vscale_pfn(c, yuv2plane1, yuv2planeX, yuv2nv12cX);
        }

        {
            for (i = vStart; i < vEnd; ++i)
                desc[i].process(c, &desc[i], dstY, 1);
        }
    }

    /* store changed local vars back in the context */
    c->dstY         = dstY;
    c->lumBufIndex  = lumBufIndex;
    c->chrBufIndex  = chrBufIndex;
    c->lastInLumBuf = lastInLumBuf;
    c->lastInChrBuf = lastInChrBuf;

    return dstY - lastDstY;
}

// bilinear 初始化
int sws_init_context(SwsContext *c)
{
    int srcW              = c->srcW;
    int srcH              = c->srcH;
    int dstW              = c->dstW;
    int dstH              = c->dstH;
    int dst_stride        = FFALIGN(dstW * sizeof(int16_t) + 66, 16);
    int flags             = 2;
    const AVPixFmtDescriptor *desc_src;
    const AVPixFmtDescriptor *desc_dst;
    int ret = 0;

    // YUV格式描述，后面尝试优化
    desc_src = av_pix_fmt_desc_get(c->srcFormat);
    desc_dst = av_pix_fmt_desc_get(c->dstFormat);


    c->lumXInc      = (((int64_t)srcW << 16) + (dstW >> 1)) / dstW;  //宽和高的缩放系数
    c->lumYInc      = (((int64_t)srcH << 16) + (dstH >> 1)) / dstH;
    c->dstFormatBpp = av_get_bits_per_pixel(desc_dst);
    c->srcFormatBpp = av_get_bits_per_pixel(desc_src);

    c->chrSrcW = AV_CEIL_RSHIFT(srcW, c->chrSrcHSubSample);
    c->chrSrcH = AV_CEIL_RSHIFT(srcH, c->chrSrcVSubSample);
    c->chrDstW = AV_CEIL_RSHIFT(dstW, c->chrDstHSubSample);
    c->chrDstH = AV_CEIL_RSHIFT(dstH, c->chrDstVSubSample);

    // // FF_ALLOCZ_OR_GOTO(c, c->formatConvBuffer, FFALIGN(srcW*2+78, 16) * 2, fail);
    // c->formatConvBuffer = av_mallocz(FFALIGN(srcW*2+78, 16) * 2);

    c->srcBpc = desc_src->comp[0].depth;
    if (c->srcBpc < 8)
        c->srcBpc = 8;
    c->dstBpc = desc_dst->comp[0].depth;
    if (c->dstBpc < 8)
        c->dstBpc = 8;
    if (c->dstBpc == 16)
        dst_stride <<= 1;
    //UV通道的缩放系数计算
    c->chrXInc = (((int64_t)c->chrSrcW << 16) + (c->chrDstW >> 1)) / c->chrDstW;
    c->chrYInc = (((int64_t)c->chrSrcH << 16) + (c->chrDstH >> 1)) / c->chrDstH;

    {
        if ((ret = initFilter(&c->hLumFilter, &c->hLumFilterPos,
                        &c->hLumFilterSize, c->lumXInc,
                        srcW, dstW, 4, 1 << 14,
                        flags,
                        get_local_pos(c, 0, 0, 0),
                        get_local_pos(c, 0, 0, 0))) < 0)
            return -1;
        if ((ret = initFilter(&c->hChrFilter, &c->hChrFilterPos,
                        &c->hChrFilterSize, c->chrXInc,
                        c->chrSrcW, c->chrDstW, 4, 1 << 14,
                        flags,
                        get_local_pos(c, c->chrSrcHSubSample, c->src_h_chr_pos, 0),
                        get_local_pos(c, c->chrDstHSubSample, c->dst_h_chr_pos, 0))) < 0)
            return -1;
    } // initialize horizontal stuff

    /* precalculate vertical scaler filter coefficients */
    {
        if ((ret = initFilter(&c->vLumFilter, &c->vLumFilterPos, &c->vLumFilterSize,
                       c->lumYInc, srcH, dstH, 2, (1 << 12),
                       flags,
                       get_local_pos(c, 0, 0, 1),
                       get_local_pos(c, 0, 0, 1))) < 0)
            return -1;
        if ((ret = initFilter(&c->vChrFilter, &c->vChrFilterPos, &c->vChrFilterSize,
                       c->chrYInc, c->chrSrcH, c->chrDstH,
                       2, (1 << 12),
                       flags,
                       get_local_pos(c, c->chrSrcVSubSample, c->src_v_chr_pos, 1),
                       get_local_pos(c, c->chrDstVSubSample, c->dst_v_chr_pos, 1))) < 0)

            return -1;
    }

    if(c->chrDstH > dstH){
        printf("dstH illegal!!\n");
        return -1;
    }
    //上面是SwsContext的参数初始化
    // 真正函数初始化
    sws_init_swscale(c);
    c->swscale = swscale; //ff_getSwsFunc(c);
    return ff_init_filters(c);

}
