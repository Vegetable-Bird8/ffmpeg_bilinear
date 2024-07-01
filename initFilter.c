#include "limits.h"
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "mem.h"
#include "swscale_internal.h"

static inline void nvXXtoUV_c(uint8_t *dst1, uint8_t *dst2,
                                        const uint8_t *src, int width)
{
    int i;
    for (i = 0; i < width; i++) {
        dst1[i] = src[2 * i + 0];
        dst2[i] = src[2 * i + 1];
    }
}

static void nv12ToUV_c(uint8_t *dstU, uint8_t *dstV,
                       const uint8_t *unused0, const uint8_t *src1, const uint8_t *src2,
                       int width)
{
    nvXXtoUV_c(dstU, dstV, src1, width);
}

static void nv21ToUV_c(uint8_t *dstU, uint8_t *dstV,
                       const uint8_t *unused0, const uint8_t *src1, const uint8_t *src2,
                       int width)
{
    nvXXtoUV_c(dstV, dstU, src1, width);
}

static inline const uint8_t clip_uint8(int a)
{
    if (a&(~0xFF)) return (~a)>>31;
    else           return a;
}

static void yuv2planeX_8_c(const int16_t *filter, int filterSize,
                           const int16_t **src, uint8_t *dest, int dstW)
{
    int i;
    for (i=0; i<dstW; i++) {
        int val = 64 << 12;
        int j;
        for (j=0; j<filterSize; j++)
            val += src[j][i] * filter[j];

        dest[i]= clip_uint8(val>>19);
    }
}

static void yuv2plane1_8_c(const int16_t *src, uint8_t *dest, int dstW)
{
    int i;
    for (i=0; i<dstW; i++) {
        int val = (src[i] + 64) >> 7;
        dest[i]= clip_uint8(val);
    }
}

static void yuv2nv12cX_c(SwsContext *c, const int16_t *chrFilter, int chrFilterSize,
                        const int16_t **chrUSrc, const int16_t **chrVSrc,
                        uint8_t *dest, int chrDstW)
{
    int i;

    if (c->dstFormat == AV_PIX_FMT_NV12)
        for (i=0; i<chrDstW; i++) {
            int u = 64 << 12;
            int v = 64 << 12;
            int j;
            for (j=0; j<chrFilterSize; j++) {
                u += chrUSrc[j][i] * chrFilter[j];
                v += chrVSrc[j][i] * chrFilter[j];
            }

            dest[2*i]= clip_uint8(u>>19);
            dest[2*i+1]= clip_uint8(v>>19);
        }
    else
        for (i=0; i<chrDstW; i++) {
            int u = 64 << 12;
            int v = 64 << 12;
            int j;
            for (j=0; j<chrFilterSize; j++) {
                u += chrUSrc[j][i] * chrFilter[j];
                v += chrVSrc[j][i] * chrFilter[j];
            }

            dest[2*i]= clip_uint8(v>>19);
            dest[2*i+1]= clip_uint8(u>>19);
        }
}
/*
这个`initFilter`函数接受多个参数，下面是每个参数的作用解释：

1. **outFilter**: 一个指向指针的指针，用于存储生成的滤波器系数。
2. **filterPos**: 一个指向指针的指针，用于存储滤波器位置信息。
3. **outFilterSize**: 一个整数指针，用于存储生成的滤波器大小。
4. **xInc**: 缩放系数
5. **srcW**: 表示源图像的宽度。
6. **dstW**: 表示目标图像的宽度。
7. **filterAlign**: 表示滤波器对齐方式，和cpu有关，x86架构下已经写死
8. **one**: 一个整数，用于计算滤波器系数。

这些参数的作用如下：
- **outFilter** 和 **filterPos** 用于存储生成的滤波器系数和位置信息，以便后续使用。
- **outFilterSize** 用于存储生成的滤波器大小，以便后续使用。
- **xInc** 用于确定水平方向的增量，影响滤波器的计算方式。
- **srcW** 和 **dstW** 分别表示源图像和目标图像的宽度，影响滤波器的大小和位置计算。
- **filterAlign** 表示滤波器的对齐方式，影响滤波器大小的调整。
- **one** 用于计算滤波器系数，影响滤波器的精度和归一化。

这些参数共同影响了滤波器的生成和调整过程，确保生成的滤波器在图像缩放过程中能够正确应用并产生良好的效果。
*/
static int initFilter(int16_t **outFilter, int32_t **filterPos,
                      int *outFilterSize, int xInc, int srcW,
                      int dstW, int filterAlign, int one)
{
    int i;
    int filterSize;
    int filter2Size;
    int minFilterSize;
    int64_t *filter = NULL;
    int64_t *filter2 = NULL;
    const int64_t fone = 1LL << (54 - FFMIN((int)log2(srcW/dstW), 8)); // 精度相关的常量
    int ret = -1;

    // 分配内存
    *filterPos = av_malloc_array((dstW + 3), sizeof(**filterPos));

    int64_t xDstInSrc;
    int sizeFactor = 2; // 双线性插值的 sizeFactor=2

    if (xInc <= 1 << 16)
        filterSize = 1 + sizeFactor; // 上采样
    else
        filterSize = 1 + (sizeFactor * srcW + dstW - 1) / dstW; // 下采样

    filterSize = FFMIN(filterSize, srcW - 2);
    filterSize = FFMAX(filterSize, 1);

    filter = av_malloc_array(dstW, sizeof(*filter) * filterSize);

    xDstInSrc = ((128 *(int64_t)xInc)>>7) - ((128 *0x10000LL)>>7); // 统一扩大系数，防止小数计算
    for (i = 0; i < dstW; i++) {
        int xx = (xDstInSrc - (filterSize - 2) * (1LL<<16)) / (1 << 17); // 计算对应方向的坐标
        (*filterPos)[i] = xx; // 存入位置矩阵中
        for (int j = 0; j < filterSize; j++) {
            int64_t d = (FFABS(((int64_t)xx * (1 << 17)) - xDstInSrc)) << 13;
            double floatd;
            int64_t coeff;

            if (xInc > 1 << 16) // 下采样
                d = d * dstW / srcW;
            floatd = d * (1.0 / (1 << 30)); // 计算和最邻近像素位置的距离

            // 双线性插值
            coeff = (1 << 30) - d;
            if (coeff < 0)
                coeff = 0;
            coeff *= fone >> 30; // 与缩放系数有关

            filter[i * filterSize + j] = coeff; // 协方差系数存入 filter 中
            xx++;
        }
        xDstInSrc += 2 * xInc;
    }

    filter2Size = filterSize;

    // 分配内存
    filter2 = av_malloc_array(dstW, sizeof(*filter2) * filter2Size);
    for (i = 0; i < dstW; i++) {
        for (int j = 0; j < filterSize; j++)
            filter2[i * filter2Size + j] = filter[i * filterSize + j];

        (*filterPos)[i] += (filterSize - 1) / 2 - (filter2Size - 1) / 2;
    }
    av_freep(&filter);

    /* 尝试减小滤波器大小 */
    // 应用一个近似归一化的滤波器
    minFilterSize = 0;
    for (i = dstW - 1; i >= 0; i--) {
        int min = filter2Size;
        int j;
        int64_t cutOff = 0.0;

        // 通过左移来消除左侧接近零的元素
        for (j = 0; j < filter2Size; j++) {
            int k;
            cutOff += FFABS(filter2[i * filter2Size]);

            if (cutOff > SWS_MAX_REDUCE_CUTOFF * fone)
                break;

            if (i < dstW - 1 && (*filterPos)[i] >= (*filterPos)[i + 1])
                break;

            // 将滤波器系数向左移动
            for (k = 1; k < filter2Size; k++)
                filter2[i * filter2Size + k - 1] = filter2[i * filter2Size + k];
            filter2[i * filter2Size + k - 1] = 0;
            (*filterPos)[i]++;
        }

        cutOff = 0;
        // 计算右侧接近零的元素
        for (j = filter2Size - 1; j > 0; j--) {
            cutOff += FFABS(filter2[i * filter2Size + j]);

            if (cutOff > SWS_MAX_REDUCE_CUTOFF * fone)
                break;
            min--;
        }

        if (min > minFilterSize)
            minFilterSize = min;
    }

    // 调整滤波器大小
    filterSize = (minFilterSize + (filterAlign - 1)) & (~(filterAlign - 1));

    // 分配内存
    filter = av_malloc_array(dstW, filterSize * sizeof(*filter));
    if (!filter)
        goto fail;

    *outFilterSize = filterSize; // 设置输出滤波器大小

    /* 进一步减小滤波器大小 */
    for (i = 0; i < dstW; i++) {
        int j;

        for (j = 0; j < filterSize; j++) {
            if (j >= filter2Size)
                filter[i * filterSize + j] = 0;
            else
                filter[i * filterSize + j] = filter2[i * filter2Size + j];
        }
    }

    // 进行一些处理，确保滤波器位置合法
    for (i = 0; i < dstW; i++) {
        int j;
        if ((*filterPos)[i] < 0) {
            for (j = 1; j < filterSize; j++) {
                int left = FFMAX(j + (*filterPos)[i], 0);
                filter[i * filterSize + left] += filter[i * filterSize + j];
                filter[i * filterSize + j] = 0;
            }
            (*filterPos)[i] = 0;
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

            (*filterPos)[i] -= shift;
            filter[i * filterSize + srcW - 1 - (*filterPos)[i]] += acc;
        }
    }

    // 分配内存
    *outFilter = av_malloc_array(dstW + 3, *outFilterSize * sizeof(int16_t));
    // 归一化并存储到 outFilter 中
    for (i = 0; i < dstW; i++) {
        int j;
        int64_t error = 0;
        int64_t sum = 0;

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
            int intV = ROUNDED_DIV(v, sum);
            (*outFilter)[i * (*outFilterSize) + j] = intV;
            error = v - intV * sum;
        }
    }

    // 边界处理
    (*filterPos)[dstW + 0] = (*filterPos)[dstW + 1] = (*filterPos)[dstW + 2] = (*filterPos)[dstW - 1];
    for (i = 0; i < *outFilterSize; i++) {
        int k = (dstW - 1) * (*outFilterSize) + i;
        (*outFilter)[k + 1 * (*outFilterSize)] = (*outFilter)[k + 2 * (*outFilterSize)] = (*outFilter)[k + 3 * (*outFilterSize)] = (*outFilter)[k];
    }

    ret = 0;

fail:
    if (ret < 0)
        printf("sws: initFilter failed\n");
    free(filter);
    free(filter2);
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

static int swscale(SwsContext *c, const uint8_t *src[],
                   int srcStride[], int srcSliceY,
                   int srcSliceH, uint8_t *dst[], int dstStride[])
{
    /* load a few things into local vars to make the code more readable?
     * and faster */
    const int dstW                   = c->dstW;
    const int dstH                   = c->dstH;

    const enum AVPixelFormat dstFormat = c->dstFormat;
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

        for (i = vStart; i < vEnd; ++i)
            desc[i].process(c, &desc[i], dstY, 1);  //真正处理的地方
    }

    /* store changed local vars back in the context */
    c->dstY         = dstY;
    c->lumBufIndex  = lumBufIndex;
    c->chrBufIndex  = chrBufIndex;
    c->lastInLumBuf = lastInLumBuf;
    c->lastInChrBuf = lastInChrBuf;

    return dstY - lastDstY;
}

// bilinear 初始化 局部参数会让运算速度变快
int sws_init_context(SwsContext *c)
{
    int srcW              = c->srcW;
    int srcH              = c->srcH;
    int dstW              = c->dstW;
    int dstH              = c->dstH;
    // int flags             = 2;
    const AVPixFmtDescriptor *desc_src;
    const AVPixFmtDescriptor *desc_dst;
    int ret = 0;

    // YUV格式描述，后面尝试优化
    desc_src = av_pix_fmt_desc_get(c->srcFormat);
    desc_dst = av_pix_fmt_desc_get(c->dstFormat);

    //宽和高的缩放系数
    c->lumXInc      = (((int64_t)srcW << 16) + (dstW >> 1)) / dstW;
    c->lumYInc      = (((int64_t)srcH << 16) + (dstH >> 1)) / dstH;
    c->dstFormatBpp = av_get_bits_per_pixel(desc_dst);
    c->srcFormatBpp = av_get_bits_per_pixel(desc_src);
    //色度的宽和高
    c->chrSrcW = AV_CEIL_RSHIFT(srcW, c->chrSrcHSubSample);
    c->chrSrcH = AV_CEIL_RSHIFT(srcH, c->chrSrcVSubSample);
    c->chrDstW = AV_CEIL_RSHIFT(dstW, c->chrDstHSubSample);
    c->chrDstH = AV_CEIL_RSHIFT(dstH, c->chrDstVSubSample);
    //位深的计算
    c->srcBpc = desc_src->comp[0].depth;
    c->dstBpc = desc_dst->comp[0].depth;
    //UV通道的缩放系数计算
    c->chrXInc = (((int64_t)c->chrSrcW << 16) + (c->chrDstW >> 1)) / c->chrDstW;
    c->chrYInc = (((int64_t)c->chrSrcH << 16) + (c->chrDstH >> 1)) / c->chrDstH;

    {// initialize horizontal stuff 初始化水平相关的参数
        if ((ret = initFilter(&c->hLumFilter, &c->hLumFilterPos,        // 传入的是亮度滤波器、亮度滤波位置参数
                        &c->hLumFilterSize, c->lumXInc,                 // 传入的是亮度滤波器尺寸，c->lumXInc是水平缩放系数，
                        srcW, dstW, 4, 1 << 14)) < 0)                         // 源图像和目标图像的宽 对齐参数 水平常参

            return -1;
            // 水平色度滤波器参数
        if ((ret = initFilter(&c->hChrFilter, &c->hChrFilterPos,
                        &c->hChrFilterSize, c->chrXInc,
                        c->chrSrcW, c->chrDstW, 4, 1 << 14)) < 0)
            return -1;
    }

    /* precalculate vertical scaler filter coefficients 计算垂直缩放的相关系数 */
    {
        if ((ret = initFilter(&c->vLumFilter, &c->vLumFilterPos, &c->vLumFilterSize,
                       c->lumYInc, srcH, dstH, 2, (1 << 12))) < 0)
            return -1;
        if ((ret = initFilter(&c->vChrFilter, &c->vChrFilterPos, &c->vChrFilterSize,
                       c->chrYInc, c->chrSrcH, c->chrDstH,
                       2, (1 << 12))) < 0)

            return -1;
    }

    if(c->chrDstH > dstH){
        printf("dstH illegal!!\n");
        return -1;
    }
    //上面是SwsContext的参数初始化
    // 初始化输出函数，这里负责将UV转为相应的格式，比如plane和交错排列的NV格式
    c->yuv2plane1 = yuv2plane1_8_c;
    c->yuv2planeX = yuv2planeX_8_c;
    if (c->dstFormat == AV_PIX_FMT_NV12 || c->dstFormat == AV_PIX_FMT_NV21)
        c->yuv2nv12cX = yuv2nv12cX_c;
    // 初始化输入函数，主要是负责将NV12和NV21转成UV的存储方式
    c->chrToYV12 = NULL;
    if(c->srcFormat == AV_PIX_FMT_NV12)
        c->chrToYV12 = nv12ToUV_c;
    else if(c->srcFormat == AV_PIX_FMT_NV21)
        c->chrToYV12 = nv21ToUV_c;

    c->hyScale = c->hcScale = hScale8To15_c;//在这里给赋予了用来计算的函数,双线性插值
    c->needs_hcscale = 1;// 都需要垂直缩放
    c->swscale = swscale;

    return ff_init_filters(c);

}
