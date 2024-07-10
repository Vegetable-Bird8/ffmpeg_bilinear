#include "limits.h"
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "swscale_internal.h"

static inline void nvXXtoUV_c(uint8_t *dst1, uint8_t *dst2, const uint8_t *src, int width)
{
    int i;
    for (i = 0; i < width; i++) {
        dst1[i] = src[2 * i + 0];
        dst2[i] = src[2 * i + 1];
    }
}

static void nv12ToUV_c(uint8_t *dstU, uint8_t *dstV, const uint8_t *src1, int width)
{
    nvXXtoUV_c(dstU, dstV, src1, width);
}

static void nv21ToUV_c(uint8_t *dstU, uint8_t *dstV, const uint8_t *src1, int width)
{
    nvXXtoUV_c(dstV, dstU, src1, width);
}

static inline const uint8_t clip_uint8(int a)  //截断在 0-255之间
{
    if (a&(~0xFF)) return (~a)>>31;
    else           return a;
}

static void yuv2planeX_8_c(const int16_t *filter, int filterSize,
                           const int16_t **src, uint8_t *dest, int dstW)
{
    for (int i=0; i<dstW; i++) {
        int val = 64 << 12;
        for (int j=0; j<filterSize; j++)
            val += src[j][i] * filter[j];

        dest[i]= clip_uint8(val>>19);  // 之前系数放大过2^7 上面的初始值又放大了2^12 故最终右移19位
    }
}

static void yuv2plane1_8_c(const int16_t *src, uint8_t *dest, int dstW)
{
    for (int i=0; i<dstW; i++) {
        int val = (src[i] + 64) >> 7;
        dest[i]= clip_uint8(val);  //相上个函数主要是没和滤波器相乘，
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
`initFilter`参数
- **outFilter** 和 **filterPos** 用于存储生成的滤波器系数和位置信息，以便后续使用。
- **outFilterSize** 用于存储生成的滤波器大小，以便后续使用。
- **xInc** 用于确定水平方向的坐标增量，影响滤波器的计算方式。
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

    // 分配内存，给filterPos申请的大小是 宽度乘于filterPos的字节数4 ，+3防止溢出
    *filterPos = malloc((dstW + 3)*sizeof(**filterPos));

    int64_t xDstInSrc;
    int sizeFactor = 2; // 双线性插值的 sizeFactor=2

    if (xInc <= 1 << 16)
        filterSize = 1 + sizeFactor; // 上采样
    else
        filterSize = 1 + (sizeFactor * srcW + dstW - 1) / dstW; // 下采样 srcw>dstw filtersize>=4

    filterSize = FFMIN(filterSize, srcW - 2);
    filterSize = FFMAX(filterSize, 1);
    printf("filterSize: %d\n", filterSize);
    filter = malloc(dstW * sizeof(*filter) * filterSize);//filter分配的大小为 目标图像宽度 x 8 x filtersize

    xDstInSrc = xInc - 1;
    for (i = 0; i < dstW; i++) {
        int xx = (xDstInSrc - (filterSize - 2) * (1LL<<16)) / (1 << 17); // 计算对应方向的坐标，filtersize的起始位置坐标
        (*filterPos)[i] = xx; // 存入位置矩阵中，代表了目标图像该位置对应的源图像坐标
        printf("float d is :\n");
        for (int j = 0; j < filterSize; j++) {
            int64_t d = (FFABS(((int64_t)xx * (1 << 17)) - xDstInSrc)) << 13;
            double floatd;
            int64_t coeff;

            if (xInc > 1 << 16) // 下采样
                d = d * dstW / srcW;
            floatd = d * (1.0 / (1 << 30)); // 计算和最邻近像素位置的距离
            printf("%f ",floatd);
            // 双线性插值
            coeff = (1 << 30) - d;
            if (coeff < 0)
                coeff = 0;
            coeff *= fone >> 30; // 与缩放系数有关

            filter[i * filterSize + j] = coeff; // 该位置的坐标和对应位置滤波器系数之间的协方差系数存入 filter 中
            xx++;//坐标右移一位，直到filter的最后一个位置
        }
        printf("\n");
        xDstInSrc += 2 * xInc;
    }

    filter2Size = filterSize;
    printf("filter: \n");
    for (i = 0; i < dstW; i++) {
        for (int j = 0; j < filterSize; j++) {
            printf("%ld ", filter[i * filterSize + j]);
        }
        printf("\n");
    }
    printf("\n");
    printf("filterPos: \n");
    for (i = 0; i < dstW; i++) {
        printf("%d ", (*filterPos)[i]);
    }
    printf("\n");
    // 分配内存
    filter2 = malloc(dstW * sizeof(*filter2) * filter2Size);
    for (i = 0; i < dstW; i++) {
        for (int j = 0; j < filterSize; j++)
            filter2[i * filter2Size + j] = filter[i * filterSize + j];  //逐个赋值
    }
    av_freep(&filter);

    printf("filter2: \n");
    for (i = 0; i < dstW; i++) {
        for (int j = 0; j < filter2Size; j++) {
            printf("%ld ", filter2[i * filterSize + j]);
        }
        printf("\n");
    }
    printf("\n");
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
            cutOff += FFABS(filter2[i * filter2Size]);   // +=每行滤波系数的最左边值

            if (cutOff > SWS_MAX_REDUCE_CUTOFF * fone)   // 如果足够大的话 就跳过 小于等于这个值得话，就说明相关度不高，归一化之后为接近于0
                break;

            if (i < dstW - 1 && (*filterPos)[i] >= (*filterPos)[i + 1])  //如果位置系数的左值大于等于右值，也跳过（一般都是小于等于）
                break;

            // 将滤波器系数向左移动一位
            for (k = 1; k < filter2Size; k++)
                filter2[i * filter2Size + k - 1] = filter2[i * filter2Size + k];
            filter2[i * filter2Size + k - 1] = 0;   //移动完后最右侧赋0，说明右侧远端也忽略
            (*filterPos)[i]++; //同时把位置系数的起始位置更新
        }

        cutOff = 0;
        // 计算右侧接近零的元素
        for (j = filter2Size - 1; j > 0; j--) {
            cutOff += FFABS(filter2[i * filter2Size + j]);  //+=最右侧位置的值

            if (cutOff > SWS_MAX_REDUCE_CUTOFF * fone)  //  如果大于阈值，则不截断
                break;
            min--;  //否则就认为最小的filtersize能够-1
        }

        if (min > minFilterSize)
            minFilterSize = min;
    }
    printf("new filter2: \n");
    for (i = 0; i < dstW; i++) {
        for (int j = 0; j < filter2Size; j++) {
            printf("%ld ", filter2[i * filter2Size + j]);
        }
        printf("\n");
    }
    printf("\n");
    // 调整滤波器大小 使得滤波器是>=minFilterSize且能被filterAlign整除的值
    filterSize = (minFilterSize + (filterAlign - 1)) & (~(filterAlign - 1));
    printf("new filterSize: %d\n", filterSize);
    // 分配内存
    filter = malloc(dstW * filterSize * sizeof(*filter));
    if (!filter)
        goto fail;

    *outFilterSize = filterSize; // 设置输出滤波器大小

    /* 进一步减小滤波器大小 */
    for (i = 0; i < dstW; i++) {
        int j;

        for (j = 0; j < filterSize; j++) {
            if (j >= filter2Size)
                filter[i * filterSize + j] = 0; // 把超过原始filter尺寸位置的协方差都先置为0
            else
                filter[i * filterSize + j] = filter2[i * filter2Size + j];  //原始位置的就直接搬过来
        }
    }

    // 进行一些处理，确保滤波器位置合法
    for (i = 0; i < dstW; i++) {
        int j;
        if ((*filterPos)[i] < 0) { //首先检查滤波器位置是否小于0，如果是，则将滤波位置系数向右移动以确保不会超出源图像的边界。
            for (j = 1; j < filterSize; j++) {  // 从第二个位置开始检查
                int left = FFMAX(j + (*filterPos)[i], 0);  // 确定左边界>=0
                filter[i * filterSize + left] += filter[i * filterSize + j];
                filter[i * filterSize + j] = 0;   // 将位置系数>0的位置赋左移
            }
            (*filterPos)[i] = 0;   // 超出左边界的就设为左边界
        }

        if ((*filterPos)[i] + filterSize > srcW) {//如果滤波器位置加上滤波器大小超过了源图像的宽度 srcW，则需要调整滤波器位置和更新滤波器的内容，以确保滤波器不会超出源图像的右边界。
            int shift = (*filterPos)[i] + FFMIN(filterSize - srcW, 0);   // 计算偏移量
            int64_t acc = 0;

            for (j = filterSize - 1; j >= 0; j--) {  // 从右向左开始循环
                if ((*filterPos)[i] + j >= srcW) {  // 如果该位置加上偏移超出边界
                    acc += filter[i * filterSize + j];   //收集该位置的值
                    filter[i * filterSize + j] = 0;    // 并将该位置赋0
                }
            }
            for (j = filterSize - 1; j >= 0; j--) { // 从滤波器的最后一个位置开始逆序遍历
                if (j < shift) { // 如果当前位置j小于偏移量shift
                    filter[i * filterSize + j] = 0; // 将该位置的值设为0，表示超出右边界
                } else {
                    filter[i * filterSize + j] = filter[i * filterSize + j - shift];     // 将当前位置j的值设置为滤波器中当前位置减去偏移量shift的位置的值
                }                                                                        // 通过移动滤波器内容来更新滤波器位置，确保不超出源图像的右边界

            }

            (*filterPos)[i] -= shift;
            filter[i * filterSize + srcW - 1 - (*filterPos)[i]] += acc;  //srcW - 1 - (*filterPos)[i] 这部分可能是用来计算滤波器在源图像中的位置，以确保在源图像内部进行累加操作。
        }    //表示滤波器中特定位置的值，累加上 acc
    }
    printf("new filter: \n");
    for (i = 0; i < dstW; i++) {
        for (int j = 0; j < filterSize; j++) {
            printf("%ld ", filter[i * filterSize + j]);
        }
        printf("\n");
    }
    printf("\n new filterPos: \n");
    for (i = 0; i < dstW; i++) {
        printf("%d ", (*filterPos)[i]);
    }
    printf("\n");
    // 分配内存
    *outFilter = malloc((dstW + 3) * (*outFilterSize * sizeof(int16_t)));
    // 归一化并存储到 outFilter 中
    for (i = 0; i < dstW; i++) {
        int j;
        int64_t error = 0;
        int64_t sum = 0;

        for (j = 0; j < filterSize; j++) {
            sum += filter[i * filterSize + j];  //累加filter中的每个值
        }
        sum = (sum + one / 2) / one;   // one是常量，水平时是2^14，垂直时是2^12 这里的技巧也是减少误差传播，和xInc的计算一样
        if (!sum) {
            printf("SwScaler: zero vector in scaling\n");
            sum = 1;
        }
        for (j = 0; j < *outFilterSize; j++) {
            int64_t v = filter[i * filterSize + j] + error;
            int intV = ROUNDED_DIV(v, sum);     //归一化操作，并进行四舍五入
            (*outFilter)[i * (*outFilterSize) + j] = intV;  // 赋值
            error = v - intV * sum;  //处理误差
        }
    }

    // 边界处理 多分配的那部分内存赋值为最后一个坐标的值
    (*filterPos)[dstW + 0] = (*filterPos)[dstW + 1] = (*filterPos)[dstW + 2] = (*filterPos)[dstW - 1];
    for (i = 0; i < *outFilterSize; i++) {
        int k = (dstW - 1) * (*outFilterSize) + i;
        (*outFilter)[k + 1 * (*outFilterSize)] = (*outFilter)[k + 2 * (*outFilterSize)] = (*outFilter)[k + 3 * (*outFilterSize)] = (*outFilter)[k];
    }
    printf("out filter: \n");
    for (i = 0; i < dstW; i++) {
        for (int j = 0; j < *outFilterSize; j++) {
            printf("%ld ", (*outFilter)[i * (*outFilterSize) + j]);
        }
        printf("\n");
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
// 根据位置矩阵计算新像素点的像素
static void hScale8To15_c(SwsContext *c, int16_t *dst, int dstW,
                          const uint8_t *src, const int16_t *filter,
                          const int32_t *filterPos, int filterSize)
{
    for (int i = 0; i < dstW; i++) {    //对宽度进行循环
        int srcPos = filterPos[i];      //获取源图像对应坐标位置的
        int val    = 0;
        for (int j = 0; j < filterSize; j++) {   //对矩阵尺寸遍历
            val += ((int)src[srcPos + j]) * filter[filterSize * i + j];  //对应位置相乘 并相加 获得新的像素值
        }
        dst[i] = FFMIN(val >> 7, (1 << 15) - 1); // 存入输出中 并做0-255截断
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
    const int chrSrcSliceY           =                srcSliceY >> c->chrSrcVSubSample; //起始位置
    const int chrSrcSliceH           = AV_CEIL_RSHIFT(srcSliceH,   c->chrSrcVSubSample); //总行数
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
    SwsSlice *src_slice = &c->slice[lumStart];    // 第一个切片 保存源数据的
    SwsSlice *hout_slice = &c->slice[c->numSlice-2];  // 水平切片
    SwsSlice *vout_slice = &c->slice[c->numSlice-1];   // 垂直切片
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

    lastDstY = dstY;  //最后处理的行数
    //初始化垂直缩放的函数
    ff_init_vscale_pfn(c, yuv2plane1, yuv2planeX, yuv2nv12cX);
    //使用源图像src给src_slice中填数据
    ff_init_slice_from_src(src_slice, (uint8_t**)src, srcStride, c->srcW,
            srcSliceY, srcSliceH, chrSrcSliceY, chrSrcSliceH, 1);
    // 这里只初始化了最终的输出 vout_slice,使用dst的数据去填充
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
        const int chrDstY = dstY >> c->chrDstVSubSample;  // 计算色度的目标坐标

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
        //处理最后一行的时候会走到这里
        if (firstLumSrcY > lastInLumBuf) {
            // printf("handle holes lum\n");
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
            // printf("handle holes chr\n");
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
        if (posY <= lastLumSrcY && !hasLumHoles) {  // lum正常处理流程
            firstPosY = FFMAX(firstLumSrcY, posY);  // first和当前posY中取最大值
            lastPosY = FFMIN(firstLumSrcY + hout_slice->plane[0].available_lines - 1, srcSliceY + srcSliceH - 1);
        } else {
            firstPosY = posY;
            lastPosY = lastLumSrcY;
        }

        cPosY = hout_slice->plane[1].sliceY + hout_slice->plane[1].sliceH;  // chr正常处理流程
        if (cPosY <= lastChrSrcY && !hasChrHoles) {
            firstCPosY = FFMAX(firstChrSrcY, cPosY);
            lastCPosY = FFMIN(firstChrSrcY + hout_slice->plane[1].available_lines - 1, AV_CEIL_RSHIFT(srcSliceY + srcSliceH, c->chrSrcVSubSample) - 1);
        } else {
            firstCPosY = cPosY;
            lastCPosY = lastChrSrcY;
        }

        ff_rotate_slice(hout_slice, lastPosY, lastCPosY);  // 不停旋转切片，这里是环形buffer，不停将需要处理的旋转到当前位置

        if (posY < lastLumSrcY + 1) {
            for (i = lumStart; i < lumEnd; ++i)
                desc[i].process(c, &desc[i], firstPosY, lastPosY - firstPosY + 1);  //调用lum处理函数
        }

        lumBufIndex += lastLumSrcY - lastInLumBuf;
        lastInLumBuf = lastLumSrcY;

        if (cPosY < lastChrSrcY + 1) {
            for (i = chrStart; i < chrEnd; ++i)
                desc[i].process(c, &desc[i], firstCPosY, lastCPosY - firstCPosY + 1);  // 调用chr处理函数
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
            desc[i].process(c, &desc[i], dstY, 1);  //调用垂直缩放函数，并写入输出
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

    const AVPixFmtDescriptor *desc_src;
    const AVPixFmtDescriptor *desc_dst;
    int ret = 0;

    // YUV格式描述，后面尝试优化
    desc_src = av_pix_fmt_desc_get(c->srcFormat);
    desc_dst = av_pix_fmt_desc_get(c->dstFormat);

    //宽和高的缩放系数
    c->lumXInc      = (((int64_t)srcW << 16) + (dstW >> 1)) / dstW;
    c->lumYInc      = (((int64_t)srcH << 16) + (dstH >> 1)) / dstH;  //添加偏移是为了减少误差累积，使得最终结果更接近于四舍五入的结果
    c->dstFormatBpp = av_get_bits_per_pixel(desc_dst);
    c->srcFormatBpp = av_get_bits_per_pixel(desc_src);
    //色度的宽和高
    c->chrSrcW = AV_CEIL_RSHIFT(srcW, c->chrSrcHSubSample);
    c->chrSrcH = AV_CEIL_RSHIFT(srcH, c->chrSrcVSubSample);
    c->chrDstW = AV_CEIL_RSHIFT(dstW, c->chrDstHSubSample);
    c->chrDstH = AV_CEIL_RSHIFT(dstH, c->chrDstVSubSample);
    //位深的获取
    c->srcBpc = desc_src->bpc;
    c->dstBpc = desc_dst->bpc;
    //UV通道的缩放系数计算
    c->chrXInc = (((int64_t)c->chrSrcW << 16) + (c->chrDstW >> 1)) / c->chrDstW;
    c->chrYInc = (((int64_t)c->chrSrcH << 16) + (c->chrDstH >> 1)) / c->chrDstH;

    {// initialize horizontal stuff 初始化水平相关的参数
        if ((ret = initFilter(&c->hLumFilter, &c->hLumFilterPos,            // 水平亮度滤波器系数、水平亮度滤波位置参数
                        &c->hLumFilterSize, c->lumXInc,                     // 水平亮度滤波器尺寸，亮度水平缩放系数，
                        srcW, dstW, 4, 1 << 14))                            // 源图像和目标图像的宽 对齐参数 水平常参
                         < 0)
            return -1;
            // 水平色度滤波器参数
        if ((ret = initFilter(&c->hChrFilter, &c->hChrFilterPos,
                        &c->hChrFilterSize, c->chrXInc,
                        c->chrSrcW, c->chrDstW, 4, 1 << 14))
                         < 0)
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
    c->yuv2plane1 = yuv2plane1_8_c;                                         // 该函数指针主要用于仅作垂直或者水平缩放的情况
    c->yuv2planeX = yuv2planeX_8_c;                                         // 该函数在水平和垂直方向同时需要缩放的情况下调用，使用最多
    if (c->dstFormat == AV_PIX_FMT_NV12 || c->dstFormat == AV_PIX_FMT_NV21)
        c->yuv2nv12cX = yuv2nv12cX_c;                                       //该函数主要用于将UV格式写成NV的UV交错排列方式，用于输出

    c->chrToYV12 = NULL;                                                    // 初始化输入函数，主要是负责将NV12和NV21转成UV的存储方式
    if(c->srcFormat == AV_PIX_FMT_NV12)
        c->chrToYV12 = nv12ToUV_c;
    else if(c->srcFormat == AV_PIX_FMT_NV21)
        c->chrToYV12 = nv21ToUV_c;

    c->hyScale = c->hcScale = hScale8To15_c;                                //该函数利用计算好的filter filterPos和原始图像的像素值，来计算目标图像对应位置的像素值
    c->needs_hcscale = 1;// 都需要垂直缩放
    c->swscale = swscale;                                                   //初始化切片数据 调用计算函数进行最终的计算

    return ff_init_filters(c);                                              //初始化水平和垂直缩放函数

}
