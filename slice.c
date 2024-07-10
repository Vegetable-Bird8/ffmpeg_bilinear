#include "swscale_internal.h"
/// Color conversion instance data

static int lum_h_scale(SwsContext *c, SwsFilterDescriptor *desc, int sliceY, int sliceH)
{
    FilterContext *instance = desc->instance;
    int srcW = desc->src->width;
    int dstW = desc->dst->width;
    int xInc = instance->xInc;

    for (int i = 0; i < sliceH; ++i) {
        uint8_t ** src = desc->src->plane[0].line;
        uint8_t ** dst = desc->dst->plane[0].line;
        int src_pos = sliceY+i - desc->src->plane[0].sliceY;
        int dst_pos = sliceY+i - desc->dst->plane[0].sliceY;


        if (c->hcScale) {  //链接到hScale8To15_c 所有的最终像素计算都由这个函数完成
            c->hyScale(c, (int16_t*)dst[dst_pos], dstW, (const uint8_t *)src[src_pos], instance->filter,
                       instance->filter_pos, instance->filter_size);
        }

        desc->dst->plane[0].sliceH += 1;
    }

    return sliceH;
}

int ff_init_desc_hscale(SwsFilterDescriptor *desc, SwsSlice *src, SwsSlice *dst, uint16_t *filter, int * filter_pos, int filter_size, int xInc)
{
    FilterContext *li = malloc(sizeof(FilterContext));
    if (!li)
        return -12;
    // 滤波器上下文实例，用以存储滤波器系数数组filter，滤波器位置数组filter_pos和滤波器尺寸filter_size和对应平面的缩放系数xInc
    li->filter = filter;
    li->filter_pos = filter_pos;
    li->filter_size = filter_size;
    li->xInc = xInc;

    desc->instance = li;  // 放入对应平面的instance中

    desc->src = src;
    desc->dst = dst;

    desc->process = &lum_h_scale;   // 缩放函数，在这里对参数进行计算

    return 0;
}

static int chr_h_scale(SwsContext *c, SwsFilterDescriptor *desc, int sliceY, int sliceH)
{
    FilterContext *instance = desc->instance;    // 获取滤波器上下文实例
    // 计算源图像和目标图像的水平色度采样后的宽度
    int srcW = AV_CEIL_RSHIFT(desc->src->width, desc->src->h_chr_sub_sample);
    int dstW = AV_CEIL_RSHIFT(desc->dst->width, desc->dst->h_chr_sub_sample);
    // 获取滤波器水平缩放系数
    int xInc = instance->xInc;
    // 获取源图像和目标图像的色度平面数据
    uint8_t ** src1 = desc->src->plane[1].line;
    uint8_t ** dst1 = desc->dst->plane[1].line;
    uint8_t ** src2 = desc->src->plane[2].line;
    uint8_t ** dst2 = desc->dst->plane[2].line;
    // 计算源图像和目标图像的色度平面起始位置
    int src_pos1 = sliceY - desc->src->plane[1].sliceY;
    int dst_pos1 = sliceY - desc->dst->plane[1].sliceY;
    int src_pos2 = sliceY - desc->src->plane[2].sliceY;
    int dst_pos2 = sliceY - desc->dst->plane[2].sliceY;

    for (int i = 0; i < sliceH; ++i) {
        // 如果存在水平色度缩放函数指针，则进行水平色度缩放处理
        if (c->hcScale) {
            // 对色度平面1进行水平缩放
            c->hcScale(c, (uint16_t*)dst1[dst_pos1+i], dstW, src1[src_pos1+i], instance->filter, instance->filter_pos, instance->filter_size);

            // 对色度平面2进行水平缩放
            c->hcScale(c, (uint16_t*)dst2[dst_pos2+i], dstW, src2[src_pos2+i], instance->filter, instance->filter_pos, instance->filter_size);
        }
        // 增加目标图像的色度平面切片高度
        desc->dst->plane[1].sliceH += 1;
        desc->dst->plane[2].sliceH += 1;
    }

    // 返回处理的切片高度
    return sliceH;
}


static int chr_convert(SwsContext *c, SwsFilterDescriptor *desc, int sliceY, int sliceH)
{
    int srcW = AV_CEIL_RSHIFT(desc->src->width, desc->src->h_chr_sub_sample);// 计算源图像的实际宽度

    int sp0 = (sliceY - (desc->src->plane[0].sliceY >> desc->src->v_chr_sub_sample)) << desc->src->v_chr_sub_sample;    // 计算色度平面的起始位置
    int sp1 = sliceY - desc->src->plane[1].sliceY;

    int i;

    desc->dst->plane[1].sliceY = sliceY; // 设置目标图像的色度平面的切片起始位置和切片高度
    desc->dst->plane[1].sliceH = sliceH;
    desc->dst->plane[2].sliceY = sliceY;
    desc->dst->plane[2].sliceH = sliceH;

    for (i = 0; i < sliceH; ++i) {
        const uint8_t * src[4] = { desc->src->plane[0].line[sp0+i],// 获取源图像的数据
                        desc->src->plane[1].line[sp1+i],
                        desc->src->plane[2].line[sp1+i],
                        desc->src->plane[3].line[sp0+i]};

        // 获取目标图像的色度平面的行数据
        uint8_t * dst1 = desc->dst->plane[1].line[i];
        uint8_t * dst2 = desc->dst->plane[2].line[i];
        if (c->chrToYV12) {
            c->chrToYV12(dst1, dst2, src[1], srcW);// 如果存在色度转换函数指针，则进行色度转换并写入目标图像的色度平面
        }
    }

    // 返回处理的切片高度
    return sliceH;
}


int ff_init_desc_cfmt_convert(SwsFilterDescriptor *desc, SwsSlice * src, SwsSlice *dst)
{
    desc->src = src;
    desc->dst = dst;
    desc->process = &chr_convert;
    return 0;
}

int ff_init_desc_chscale(SwsFilterDescriptor *desc, SwsSlice *src, SwsSlice *dst, uint16_t *filter, int * filter_pos, int filter_size, int xInc)
{
    FilterContext *li = malloc(sizeof(FilterContext));
    if (!li)
        return -12;

    li->filter = filter;
    li->filter_pos = filter_pos;
    li->filter_size = filter_size;
    li->xInc = xInc;

    desc->instance = li;

    desc->src = src;
    desc->dst = dst;

    desc->process = &chr_h_scale;

    return 0;
}

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
        ((yuv2planar1_fn)inst->pfn)((const int16_t*)src[0], dst[0], dstW);
    else
        ((yuv2planarX_fn)inst->pfn)(filter, inst->filter_size, (const int16_t**)src, dst[0], dstW);

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
            ((yuv2planar1_fn)inst->pfn)((const int16_t*)src1[0], dst1[0], dstW);
            ((yuv2planar1_fn)inst->pfn)((const int16_t*)src2[0], dst2[0], dstW);
        } else {
            ((yuv2planarX_fn)inst->pfn)(filter, inst->filter_size, (const int16_t**)src1, dst1[0], dstW);
            ((yuv2planarX_fn)inst->pfn)(filter, inst->filter_size, (const int16_t**)src2, dst2[0], dstW);
        }
    }

    return 1;
}

int ff_init_vscale(SwsContext *c, SwsFilterDescriptor *desc, SwsSlice *src, SwsSlice *dst)
{
    VScalerContext *lumCtx = NULL;
    VScalerContext *chrCtx = NULL;

    lumCtx = av_mallocz(sizeof(VScalerContext));
    if (!lumCtx)
        return -12;


    desc[0].process = lum_planar_vscale;
    desc[0].instance = lumCtx;
    desc[0].src = src;
    desc[0].dst = dst;

    chrCtx = av_mallocz(sizeof(VScalerContext));
    if (!chrCtx)
        return -12;  // -12为内存分配失败
    desc[1].process = chr_planar_vscale;
    desc[1].instance = chrCtx;
    desc[1].src = src;
    desc[1].dst = dst;

    ff_init_vscale_pfn(c, c->yuv2plane1, c->yuv2planeX, c->yuv2nv12cX);
    return 0;
}

void ff_init_vscale_pfn(SwsContext *c,
    yuv2planar1_fn yuv2plane1,
    yuv2planarX_fn yuv2planeX,
    yuv2interleavedX_fn yuv2nv12cX)
{
    VScalerContext *lumCtx = NULL;  // 亮度缩放上下文
    VScalerContext *chrCtx = NULL;  // 色度缩放上下文
    int idx = c->numDesc - 1;  // 获取描述符数组的索引

    chrCtx = c->desc[idx].instance;  // 获取色度缩放上下文实例

    // 设置色度滤波器和相关参数
    chrCtx->filter[0] = c->vChrFilter;
    chrCtx->filter_size = c->vChrFilterSize;
    chrCtx->filter_pos = c->vChrFilterPos;

    --idx;  // 减小索引值

    // 根据条件设置色度缩放函数指针
    if (yuv2nv12cX)
        chrCtx->pfn = yuv2nv12cX;
    else if (c->vChrFilterSize == 1)
        chrCtx->pfn = yuv2plane1;
    else
        chrCtx->pfn = yuv2planeX;

    lumCtx = c->desc[idx].instance;  // 获取亮度缩放上下文实例

    // 设置亮度滤波器和相关参数
    lumCtx->filter[0] = c->vLumFilter;
    lumCtx->filter[1] = c->vLumFilter;
    lumCtx->filter_size = c->vLumFilterSize;
    lumCtx->filter_pos = c->vLumFilterPos;

    // 根据条件设置亮度缩放函数指针
    if (c->vLumFilterSize == 1)
        lumCtx->pfn = yuv2plane1;
    else
        lumCtx->pfn = yuv2planeX;
}



static void free_lines(SwsSlice *s)
{
    int i;
    for (i = 0; i < 2; ++i) {
        int n = s->plane[i].available_lines;
        int j;
        for (j = 0; j < n; ++j) {
            av_freep(&s->plane[i].line[j]);
            if (s->is_ring)
               s->plane[i].line[j+n] = NULL;
        }
    }

    for (i = 0; i < 4; ++i)
        memset(s->plane[i].line, 0, sizeof(uint8_t*) * s->plane[i].available_lines * (s->is_ring ? 3 : 1));
    s->should_free_lines = 0;
}

/*
 切片行包含额外的字节用于矢量代码，因此@size是分配的内存大小，@width是像素数
*/
static int alloc_lines(SwsSlice *s, int size, int width)
{
    int i;
    int idx[2] = {3, 2};            // 索引数组，用以拼接数据，当i=0时 ii=3； i=1时，ii=2

    s->should_free_lines = 1;       // 标记需要释放行内存
    s->width = width;               // 图像宽度

    for (i = 0; i < 2; ++i) {
        int n = s->plane[i].available_lines;    // 获取可用行数
        int ii = idx[i];                        // 获取平面索引  i=0时 ii=3

        for (int j = 0; j < n; ++j) {
            // 为色度平面分配内存，确保U和V在内存中是连续存储的
            s->plane[i].line[j] = malloc(size * 2 + 32); // 分配内存
            if (!s->plane[i].line[j]) {
                free_lines(s); // 释放内存
                return -12; // 内存分配失败
            }
            s->plane[ii].line[j] = s->plane[i].line[j] + size + 16; // 设置平面ii的行指针，将第三个平面指向第一个平面的后面一半的头指针，将第二个平面指向第三个平面后面一半的头指针
            if (s->is_ring) {
                s->plane[i].line[j+n] = s->plane[i].line[j]; // 环形缓冲处理
                s->plane[ii].line[j+n] = s->plane[ii].line[j];
            }
        }
    }

    return 0; // 成功分配内存
}


static int alloc_slice(SwsSlice *s, enum AVPixelFormat fmt, int lumLines, int chrLines, int h_sub_sample, int v_sub_sample, int ring)
{
    int i;
    int size[4] = { lumLines, chrLines, chrLines, lumLines }; // 亮度行数、色度行数

    s->h_chr_sub_sample = h_sub_sample; // 水平色度子采样因子
    s->v_chr_sub_sample = v_sub_sample; // 垂直色度子采样因子
    s->fmt = fmt;                       // 像素格式
    s->is_ring = ring;                  // 是否是环形缓冲
    s->should_free_lines = 0;           // 是否需要释放行内存

    for (i = 0; i < 4; ++i) {
        int n = size[i] * (ring == 0 ? 1 : 3); // 计算每个平面需要的行数 如果环形缓冲区的化就×3，否则保持原始大小
        s->plane[i].line = av_mallocz_array(sizeof(uint8_t*), n); // 分配内存用于存储行数据，一共n行，即该平面的行数
        if (!s->plane[i].line)
            return -12; // 内存分配失败
        s->plane[i].available_lines = size[i]; // 最大可用行数，也就是有效的行数
        s->plane[i].sliceY = 0;                 // 切片的起始行
        s->plane[i].sliceH = 0;                 // 切片的行数
    }
    return 0; // 成功分配内存
}


static void free_slice(SwsSlice *s)
{
    int i;
    if (s) {
        if (s->should_free_lines)
            free_lines(s);
        for (i = 0; i < 4; ++i) {
            av_freep(&s->plane[i].line);
            // s->plane[i].tmp = NULL;
        }
    }
}

int ff_rotate_slice(SwsSlice *s, int lum, int chr)
{
    int i;
    if (lum) {
        for (i = 0; i < 4; i+=3) {
            int n = s->plane[i].available_lines;
            int l = lum - s->plane[i].sliceY;

            if (l >= n * 2) {
                s->plane[i].sliceY += n;
                s->plane[i].sliceH -= n;
            }
        }
    }
    if (chr) {
        for (i = 1; i < 3; ++i) {
            int n = s->plane[i].available_lines;
            int l = chr - s->plane[i].sliceY;

            if (l >= n * 2) {
                s->plane[i].sliceY += n;
                s->plane[i].sliceH -= n;
            }
        }
    }
    return 0;
}

/*
 从源数据初始化图像切片结构体
 @param s SwsSlice结构体指针，要初始化的图像切片信息
 @param src 源数据的指针数组，包含4个平面的数据指针
 @param stride 源数据的步长数组，对应每个平面的步长
 @param srcW 源数据的宽度
 @param lumY 亮度平面的起始Y坐标
 @param lumH 亮度平面的高度
 @param chrY 色度平面的起始Y坐标
 @param chrH 色度平面的高度
 @param relative 是否相对位置，0表示绝对位置，1表示相对位置
 @return 返回0表示初始化成功
*/
int ff_init_slice_from_src(SwsSlice *s, uint8_t *src[4], int stride[4], int srcW, int lumY, int lumH, int chrY, int chrH, int relative)
{
    int i = 0;

    const int start[4] = {lumY, chrY, chrY, lumY}; // 四个平面的起始Y坐标

    const int end[4] = {lumY + lumH, chrY + chrH, chrY + chrH, lumY + lumH}; // 四个平面的结束Y坐标

    uint8_t *const src_[4] = {
        src[0] + (relative ? 0 : start[0]) * stride[0],
        src[1] + (relative ? 0 : start[1]) * stride[1],
        src[2] + (relative ? 0 : start[2]) * stride[2],
        src[3] + (relative ? 0 : start[3]) * stride[3]
    };

    s->width = srcW; // 设置图像切片的宽度,也是每行的像素个数

    for (i = 0; i < 4; ++i) {
        int j;
        int first = s->plane[i].sliceY;                 // 当前平面的切片起始Y坐标
        int n = s->plane[i].available_lines;            // 当前平面可用行数
        int lines = end[i] - start[i];                  // 当前平面的行数
        int tot_lines = end[i] - first;                 // 需要处理的行数，因为可能存在不是从第一行开始处理的情况

        if (start[i] >= first && n >= tot_lines) {      //如果总行数大于等于需要处理的行数
            s->plane[i].sliceH = FFMAX(tot_lines, s->plane[i].sliceH); // 设置切片的高度
            for (j = 0; j < lines; j += 1)
                s->plane[i].line[start[i] - first + j] = src_[i] + j * stride[i]; // 填充切片数据
        }
        else {  // 目前处理的情况一般走不到这里
            s->plane[i].sliceY = start[i]; // 设置切片的起始Y坐标
            lines = lines > n ? n : lines;
            s->plane[i].sliceH = lines; // 设置切片的高度
            for (j = 0; j < lines; j += 1)
                s->plane[i].line[j] = src_[i] + j * stride[i]; // 填充切片数据
        }
    }

    return 0; // 返回0表示初始化成功
}


/*
 填充数据的函数，根据is16bit参数选择填充int32_t或int16_t类型的数据
 @param s SwsSlice结构体指针，包含了要填充数据的信息
 @param n 要填充的数据数量
 @param is16bit 是否填充16位数据，1表示填充16位数据，0表示填充32位数据
*/
static void fill_ones(SwsSlice *s, int n, int is16bit)
{
    for (int i = 0; i < 4; ++i) {                           // 遍历4个平面
        int j;
        int size = s->plane[i].available_lines;         // 获取当前平面可用行数
        for (j = 0; j < size; ++j) {                    // 遍历每一行
            int k;                                      // 计算要填充的数据结束位置
            int end = is16bit ? n >> 1 : n;
            end += 1;                                   // 在每行末尾填充一个额外元素

            if (is16bit) {                              // 根据is16bit参数选择填充数据类型
                for (k = 0; k < end; ++k)               // 填充int32_t类型数据
                    ((int32_t*)(s->plane[i].line[j]))[k] = 1 << 18;
            } else {
                for (k = 0; k < end; ++k)               // 填充int16_t类型数据
                    ((int16_t*)(s->plane[i].line[j]))[k] = 1 << 14;
            }
        }
    }
}

/*
 计算最小环形缓冲区大小，应该能够存储 vFilterSize 多个 n 行，其中 n 是每个相邻切片之间输出一行的最大差异。
 当没有足够的源行来输出单个目标行时，需要这 n 行，然后我们应该缓冲这些行以在下一次调用缩放时处理它们。
*/
static void get_min_buffer_size(SwsContext *c, int *out_lum_size, int *out_chr_size)
{
    int lumY;
    int dstH = c->dstH; // 目标图像高度
    int chrDstH = c->chrDstH; // 色度目标图像高度
    int *lumFilterPos = c->vLumFilterPos; // 亮度滤波器位置数组
    int *chrFilterPos = c->vChrFilterPos; // 色度滤波器位置数组
    int lumFilterSize = c->vLumFilterSize; // 亮度滤波器大小
    int chrFilterSize = c->vChrFilterSize; // 色度滤波器大小
    int chrSubSample = c->chrSrcVSubSample; // 色度源垂直采样比

    *out_lum_size = lumFilterSize;// 初始化为滤波器大小
    *out_chr_size = chrFilterSize;

    for (lumY = 0; lumY < dstH; lumY++) {   // 对亮度的行进行遍历
        int chrY = (int64_t)lumY * chrDstH / dstH; // 计算对应的色度行，
        int nextSlice = FFMAX(lumFilterPos[lumY] + lumFilterSize - 1,
                              ((chrFilterPos[chrY] + chrFilterSize - 1) << chrSubSample));
        //保证取整
        nextSlice >>= chrSubSample;
        nextSlice <<= chrSubSample;
        (*out_lum_size) = FFMAX((*out_lum_size), nextSlice - lumFilterPos[lumY]); // 更新亮度buffer大小
        (*out_chr_size) = FFMAX((*out_chr_size), (nextSlice >> chrSubSample) - chrFilterPos[chrY]); // 更新色度buffer大小
    }
}



int ff_init_filters(SwsContext *c)
{
    int i;
    int index;                                      // 索引
    int need_chr_conv = c->chrToYV12;               // 是否需要色度转换，NV12和NV21情况下需要
    int num_ydesc = 1;                              // 亮度描述符数量
    int num_cdesc = need_chr_conv ? 2 : 1;          // 色度描述符数量
    int num_vdesc = 2;                              // 垂直描述符数量 支持的四种格式都是YUV且Planar格式，因此固定为2


    int srcIdx, dstIdx;
    int dst_stride = FFALIGN(c->dstW * sizeof(int16_t) + 66, 16); // 目标图像的跨距
    int res = 0;

    int lumBufSize;
    int chrBufSize;
    // 获取最小缓冲区大小 根据缩放比例确定，缩放比例越大，滤波器尺寸就越大，所需要的lumbuffer越大
    get_min_buffer_size(c, &lumBufSize, &chrBufSize);
    lumBufSize = FFMAX(lumBufSize, c->vLumFilterSize + MAX_LINES_AHEAD);
    chrBufSize = FFMAX(chrBufSize, c->vChrFilterSize + MAX_LINES_AHEAD);

    c->numSlice = FFMAX(num_ydesc, num_cdesc) + 2;      // slice 的个数，取最大值并 +2，前面的用以存储输入相关的，后面+2用以存储输出相关的切片
    c->numDesc = num_ydesc + num_cdesc + num_vdesc;     //描述符的大小，由lum chr和垂直描述符个数和决定
    c->descIndex[0] = num_ydesc;                        // lum结束索引
    c->descIndex[1] = num_ydesc + num_cdesc;            // chr结束索引

    // 分配描述符和切片内存
    c->desc = av_mallocz_array(sizeof(SwsFilterDescriptor), c->numDesc);
    if (!c->desc)
        return -12;   // -12表示分配失败
    c->slice = av_mallocz_array(sizeof(SwsSlice), c->numSlice);

    // 初始化第一个切片，也是用以存储输入数据的地方
    // 这个分配的内存是最大的，为输入数据分配数据
    res = alloc_slice(&c->slice[0], c->srcFormat, c->srcH, c->chrSrcH, c->chrSrcHSubSample, c->chrSrcVSubSample, 0);
    if (res < 0) goto cleanup;

    // 初始化中间过程的切片数据
    for (i = 1; i < c->numSlice - 2; ++i) {
        res = alloc_slice(&c->slice[i], c->srcFormat, lumBufSize, chrBufSize, c->chrSrcHSubSample, c->chrSrcVSubSample, 0);  //这里lumBufSize和chrBufSize都是较小的值，会应用滚动处理，因此不需要一次性存所有的行
        if (res < 0) goto cleanup;
        res = alloc_lines(&c->slice[i], FFALIGN(c->srcW * 2 + 78, 16), c->srcW);
        if (res < 0) goto cleanup;
    }
    // 初始化输出缩放器输出切片
    res = alloc_slice(&c->slice[i], c->srcFormat, lumBufSize, chrBufSize, c->chrDstHSubSample, c->chrDstVSubSample, 1);
    if (res < 0) goto cleanup;
    res = alloc_lines(&c->slice[i], dst_stride, c->dstW);
    if (res < 0) goto cleanup;
    fill_ones(&c->slice[i], dst_stride >> 1, c->dstBpc == 16); // 填充数据

    // 初始化垂直缩放器输出切片，用以存储输出数据
    ++i;
    res = alloc_slice(&c->slice[i], c->dstFormat, c->dstH, c->chrDstH, c->chrDstHSubSample, c->chrDstVSubSample, 0);
    if (res < 0) goto cleanup;

    index = 0;
    srcIdx = 0;
    dstIdx = 1;

    dstIdx = FFMAX(num_ydesc, num_cdesc);
    res = ff_init_desc_hscale(&c->desc[index], &c->slice[srcIdx], &c->slice[dstIdx], c->hLumFilter, c->hLumFilterPos, c->hLumFilterSize, c->lumXInc);
    if (res < 0) goto cleanup;

    ++index;
    {  //色度平面的缩放器初始化 此时index=1
        srcIdx = 0;
        dstIdx = 1;

        if (need_chr_conv) {  // 输出nv12或者21的情况下才会初始化这个函数 此时后面所有的index都会+1
            res = ff_init_desc_cfmt_convert(&c->desc[index], &c->slice[srcIdx], &c->slice[dstIdx]);  //主要是将缩放好的UV平面转换成交错排列的方式
            if (res < 0) goto cleanup;
            ++index;
            srcIdx = dstIdx;
        }

        dstIdx = FFMAX(num_ydesc, num_cdesc);
        printf("srcIdx:%d \n",srcIdx);
        printf("dstIdx:%d \n",dstIdx);
        if (c->needs_hcscale)  //对色度平面进行水平缩放
            res = ff_init_desc_chscale(&c->desc[index], &c->slice[srcIdx], &c->slice[dstIdx], c->hChrFilter, c->hChrFilterPos, c->hChrFilterSize, c->chrXInc);

        if (res < 0) goto cleanup;
    }

    ++index;
    {  //垂直缩放初始化 此时index=2
        srcIdx = c->numSlice - 2;
        dstIdx = c->numSlice - 1;
        res = ff_init_vscale(c, c->desc + index, c->slice + srcIdx, c->slice + dstIdx);  // 倒数第二个作为源slice放入最后一个输出中
        if (res < 0) goto cleanup;
    }

    ++index;

    return 0;

cleanup:
    ff_free_filters(c);
    return res;
}


int ff_free_filters(SwsContext *c)
{
    int i;
    if (c->desc) {
        for (i = 0; i < c->numDesc; ++i)
            av_freep(&c->desc[i].instance);
        av_freep(&c->desc);
    }

    if (c->slice) {
        for (i = 0; i < c->numSlice; ++i)
            free_slice(&c->slice[i]);
        av_freep(&c->slice);
    }
    return 0;
}
