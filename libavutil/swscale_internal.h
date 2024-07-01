#ifndef SWSCALE_SWSCALE_INTERNAL_H
#define SWSCALE_SWSCALE_INTERNAL_H
#include <stddef.h>
#include <stdint.h>
#include "pixdesc.h"    // 像素格式描述，尝试优化

void *av_malloc(size_t size);
void *av_mallocz(size_t size);
void *av_malloc_array(size_t nmemb, size_t size);
void *av_mallocz_array(size_t nmemb, size_t size);
void free(void *ptr);
void av_freep(void *ptr);

#define MAX_FILTER_SIZE 256
#define RETCODE_USE_CASCADE -12345
#define MAX_LINES_AHEAD 4
#define SWS_ACCURATE_RND      0x40000
#define SWS_BITEXACT          0x80000
#define SWS_MAX_REDUCE_CUTOFF 0.002

#define FFMAX(a,b) ((a) > (b) ? (a) : (b))
#define FFMIN(a,b) ((a) > (b) ? (b) : (a))
#define ROUNDED_DIV(a,b) (((a)>0 ? (a) + ((b)>>1) : (a) - ((b)>>1))/(b))
#define FFALIGN(x, a) (((x)+(a)-1)&~((a)-1))
#define AV_CEIL_RSHIFT(a, b) (((a) + (1 << (b)) - 1) >> (b))

#define FFABS(a) ((a) >= 0 ? (a) : (-(a)))

typedef struct FilterContext
{
    uint16_t *filter;
    int *filter_pos;
    int filter_size;
    int xInc;
} FilterContext;

typedef struct VScalerContext
{
    uint16_t *filter[2];
    int32_t  *filter_pos;
    int filter_size;
    void *pfn;
} VScalerContext;


typedef int (*SwsFunc)(struct SwsContext *context, const uint8_t *src[],
                       int srcStride[], int srcSliceY, int srcSliceH,
                       uint8_t *dst[], int dstStride[]);

/**
    该函数将水平缩放的数据写入平面输出，同时没有进行额外的垂直缩放
 *
 * @param src     scaled source data, 15 bits for 8-10-bit output,
 *                19 bits for 16-bit output (in int32_t)
 * @param dest    pointer to the output plane. For >8-bit
 *                output, this is in uint16_t
 * @param dstW    width of destination in pixels
 */
typedef void (*yuv2planar1_fn)(const int16_t *src, uint8_t *dest, int dstW);

/**
该函数将经过水平缩放的数据写入平面输出，并在输入像素之间进行多点垂直缩放
 *
 * @param filter        vertical luma/alpha scaling coefficients, 12 bits [0,4096]
 * @param src           scaled luma (Y) or alpha (A) source data, 15 bits for
 *                      8-10-bit output, 19 bits for 16-bit output (in int32_t)
 * @param filterSize    number of vertical input lines to scale
 * @param dest          pointer to output plane. For >8-bit
 *                      output, this is in uint16_t
 * @param dstW          width of destination pixels
 * @param offset        Dither offset
 */
typedef void (*yuv2planarX_fn)(const int16_t *filter, int filterSize,
                               const int16_t **src, uint8_t *dest, int dstW);

/**
该函数将经过水平缩放的色度数据写入交错输出，并在输入像素之间进行多点垂直缩放。
 *
 * @param c             SWS scaling context
 * @param chrFilter     vertical chroma scaling coefficients, 12 bits [0,4096]
 * @param chrUSrc       scaled chroma (U) source data, 15 bits for 8-10-bit
 *                      output, 19 bits for 16-bit output (in int32_t)
 * @param chrVSrc       scaled chroma (V) source data, 15 bits for 8-10-bit
 *                      output, 19 bits for 16-bit output (in int32_t)
 * @param chrFilterSize number of vertical chroma input lines to scale
 * @param dest          pointer to the output plane. For >8-bit
 *                      output, this is in uint16_t
 * @param dstW          width of chroma planes
 */
typedef void (*yuv2interleavedX_fn)(struct SwsContext *c,
                                    const int16_t *chrFilter,
                                    int chrFilterSize,
                                    const int16_t **chrUSrc,
                                    const int16_t **chrVSrc,
                                    uint8_t *dest, int dstW);


/* This struct should be aligned on at least a 32-byte boundary. */
typedef struct SwsContext {

    SwsFunc swscale;                // 函数指针，真正做缩放的地方，之后优化
    int srcW;                     /// 源图像宽度
    int srcH;                     /// 源图像高度
    int dstH;                     ///< 目标亮度/透明度平面的高度
    int chrSrcW;                  ///< 源色度平面的宽度
    int chrSrcH;                  ///< 源色度平面的高度
    int chrDstW;                  ///< 目标色度平面的宽度
    int chrDstH;                  ///< 目标色度平面的高度
    int lumXInc, chrXInc;
    int lumYInc, chrYInc;
    enum AVPixelFormat dstFormat; ///< 目标像素格式
    enum AVPixelFormat srcFormat; ///< 源像素格式
    int dstFormatBpp;             ///< 目标像素格式的每像素位数
    int srcFormatBpp;             ///< 源像素格式的每像素位数
    int dstBpc, srcBpc;
    int chrSrcHSubSample;         ///< 源图像亮度/透明度与色度平面之间水平下采样因子的二进制对数
    int chrSrcVSubSample;         ///< 源图像亮度/透明度与色度平面之间垂直下采样因子的二进制对数
    int chrDstHSubSample;         ///< 目标图像亮度/透明度与色度平面之间水平下采样因子的二进制对数
    int chrDstVSubSample;         ///< 目标图像亮度/透明度与色度平面之间垂直下采样因子的二进制对数
    int vChrDrop;                 ///< 用户指定的源图像色度平面额外垂直下采样因子的二进制对数
    int sliceDir;                 ///< 切片被馈送到缩放器的方向（1 = 自上而下，-1 = 自下而上）

    int numDesc;
    int descIndex[2];
    int numSlice;
    struct SwsSlice *slice;
    struct SwsFilterDescriptor *desc;

    /**
     * @name 水平线缓冲区
     * 水平缩放器保留足够的缩放行在环形缓冲区中，以便它们可以传递给垂直缩放器。
     * 每行的分配缓冲区指针在环形缓冲区中依次复制，以简化索引并避免在垂直缩放器代码内部的行之间环绕。
     * 在调用垂直缩放器之前进行包装。
     */
    //@{
    int lastInLumBuf;             ///< 源图像最后一个缩放的水平亮度/透明度行在环形缓冲区中
    int lastInChrBuf;             ///< 源图像最后一个缩放的水平色度行在环形缓冲区中
    int lumBufIndex;              ///< 最后一个缩放的水平亮度/透明度行在环形缓冲区中的索引
    int chrBufIndex;              ///< 最后一个缩放的水平色度行在环形缓冲区中的索引
    //@}

    int16_t *hLumFilter;          ///< 亮度/透明度平面的水平滤波系数数组
    int16_t *hChrFilter;          ///< 色度平面的水平滤波系数数组
    int16_t *vLumFilter;          ///< 亮度/透明度平面的垂直滤波系数数组
    int16_t *vChrFilter;          ///< 色度平面的垂直滤波系数数组
    int32_t *hLumFilterPos;       ///< 每个dst[i]的亮度/透明度平面的水平滤波起始位置数组
    int32_t *hChrFilterPos;       ///< 每个dst[i]的色度平面的水平滤波起始位置数组
    int32_t *vLumFilterPos;       ///< 每个dst[i]的亮度/透明度平面的垂直滤波起始位置数组
    int32_t *vChrFilterPos;       ///< 每个dst[i]的色度平面的垂直滤波起始位置数组
    int hLumFilterSize;           ///< 亮度/透明度像素的水平滤波大小
    int hChrFilterSize;           ///< 色度像素的水平滤波大小
    int vLumFilterSize;           ///< 亮度/透明度像素的垂直滤波大小
    int vChrFilterSize;           ///< 色度像素的垂直滤波大小
    int dstW;                     ///< 目标亮度/透明度平面的宽度
    int dstY;                     ///< 最后一个从最后一个切片输出的目标垂直线
    int flags;                   ///< 用户传递的标志，选择缩放器算法、优化、子采样等...



    /* swscale()的函数指针 */
    yuv2planar1_fn yuv2plane1;
    yuv2planarX_fn yuv2planeX;
    yuv2interleavedX_fn yuv2nv12cX;

    /// 未缩放的色度平面转换为YV12用于水平缩放器。
    void (*chrToYV12)(uint8_t *dstU, uint8_t *dstV,
                      const uint8_t *src1, const uint8_t *src2, const uint8_t *src3,
                      int width);

    void (*hyScale)(struct SwsContext *c, int16_t *dst, int dstW,
                    const uint8_t *src, const int16_t *filter,
                    const int32_t *filterPos, int filterSize);
    void (*hcScale)(struct SwsContext *c, int16_t *dst, int dstW,
                    const uint8_t *src, const int16_t *filter,
                    const int32_t *filterPos, int filterSize);

    int needs_hcscale; ///< 如果有需要转换的色度平面，则设置

} SwsContext;

typedef struct SwsPlane
{
    int available_lines;    ///< max number of lines that can be hold by this plane
    int sliceY;             ///< index of first line
    int sliceH;             ///< number of lines
    uint8_t **line;         ///< line buffer
    uint8_t **tmp;          ///< Tmp line buffer used by mmx code
} SwsPlane;

/**
 * Struct which defines a slice of an image to be scaled or an output for
 * a scaled slice.
 * A slice can also be used as intermediate ring buffer for scaling steps.
 */
typedef struct SwsSlice
{
    int width;              ///< Slice line width
    int h_chr_sub_sample;   ///< horizontal chroma subsampling factor
    int v_chr_sub_sample;   ///< vertical chroma subsampling factor
    int is_ring;            ///< flag to identify if this slice is a ring buffer
    int should_free_lines;  ///< flag to identify if there are dynamic allocated lines
    enum AVPixelFormat fmt; ///< planes pixel format
    SwsPlane plane[4];   ///< color planes
} SwsSlice;

/**
 * Struct which holds all necessary data for processing a slice.
 * A processing step can be a color conversion or horizontal/vertical scaling.
 */
typedef struct SwsFilterDescriptor
{
    SwsSlice *src;  ///< Source slice
    SwsSlice *dst;  ///< Output slice
    void *instance; ///< Filter instance data

    /// Function for processing input slice sliceH lines starting from line sliceY
    int (*process)(SwsContext *c, struct SwsFilterDescriptor *desc, int sliceY, int sliceH);
} SwsFilterDescriptor;

// 以下是新加的
typedef struct AVFrame {
    uint8_t *data[4];
    unsigned int width;
    unsigned int height;
    unsigned int step;
    unsigned int elemSize;
    unsigned int subsample;

    unsigned int linesize[4];
    int Ysize;
    int UVsize;
    enum AVPixelFormat format;
}AVFrame;

int sws_init_context(SwsContext *c);  //初始化结构体


// 将形式为 (src + width*i + j) 的输入行转换为切片格式 (line[i][j])
// relative=true 表示第一行是 src[x][0]，否则第一行是 src[x][lum/crh Y]
int ff_init_slice_from_src(SwsSlice * s, uint8_t *src[4], int stride[4], int srcW, int lumY, int lumH, int chrY, int chrH, int relative);

// 初始化缩放器滤波器描述符链
int ff_init_filters(SwsContext *c);

// 释放所有滤波器数据
int ff_free_filters(SwsContext *c);

/*
 将环形缓冲逻辑应用于切片 s 的函数
 检查切片是否可以容纳更多 @lum 行，如果可以则不执行任何操作，否则移除 @lum 最不常用的行。
 对 @chr 行应用相同的过程。
*/
int ff_rotate_slice(SwsSlice *s, int lum, int chr);


/// 初始化亮度水平缩放描述符
int ff_init_desc_hscale(SwsFilterDescriptor *desc, SwsSlice *src, SwsSlice *dst, uint16_t *filter, int *filter_pos, int filter_size, int xInc);

/// 初始化色度像素格式转换描述符
int ff_init_desc_cfmt_convert(SwsFilterDescriptor *desc, SwsSlice *src, SwsSlice *dst);

/// 初始化色度水平缩放描述符
int ff_init_desc_chscale(SwsFilterDescriptor *desc, SwsSlice *src, SwsSlice *dst, uint16_t *filter, int *filter_pos, int filter_size, int xInc);


/// 初始化垂直缩放描述符
int ff_init_vscale(SwsContext *c, SwsFilterDescriptor *desc, SwsSlice *src, SwsSlice *dst);

/// 设置垂直缩放器函数
void ff_init_vscale_pfn(SwsContext *c, yuv2planar1_fn yuv2plane1, yuv2planarX_fn yuv2planeX,
    yuv2interleavedX_fn yuv2nv12cX);


/**
 * 对 srcSlice 中的图像切片进行缩放，并将结果放入 dst 中的图像切片。
 * 切片是图像中连续行的序列。
 *
 * 必须按顺序提供切片，可以是自顶向下或自底向上的顺序。如果以非顺序方式提供切片，则函数的行为是未定义的。
 *
 * @param c         先前使用 sws_getContext() 创建的缩放上下文
 * @param srcSlice  包含源切片各平面指针的数组
 * @param srcStride 包含源图像各平面跨距的数组
 * @param srcSliceY 要处理的源图像切片的位置，即切片的第一行在图像中的位置（从零开始计数）
 * @param srcSliceH 源切片的高度，即切片中的行数
 * @param dst       包含目标图像各平面指针的数组
 * @param dstStride 包含目标图像各平面跨距的数组
 * @return          输出切片的高度
 */
int sws_scale(struct SwsContext *c, const uint8_t *const srcSlice[],
              const int srcStride[], int srcSliceY, int srcSliceH,
              uint8_t *const dst[], const int dstStride[]);


#endif
/* SWSCALE_SWSCALE_INTERNAL_H */
