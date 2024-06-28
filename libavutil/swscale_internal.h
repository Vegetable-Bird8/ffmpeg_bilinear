#ifndef SWSCALE_SWSCALE_INTERNAL_H
#define SWSCALE_SWSCALE_INTERNAL_H


#include <stddef.h>
#include <stdint.h>

#include "pixdesc.h"
#include "array.c"
#include "mem.h"


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

extern const uint8_t ff_log2_tab[256];
#define av_log2 ff_log2_c
static inline const int ff_log2_c(unsigned int v)
{
    int n = 0;
    if (v & 0xffff0000) {
        v >>= 16;
        n += 16;
    }
    if (v & 0xff00) {
        v >>= 8;
        n += 8;
    }
    n += ff_log2_tab[v];

    return n;
}

typedef int (*SwsFunc)(struct SwsContext *context, const uint8_t *src[],
                       int srcStride[], int srcSliceY, int srcSliceH,
                       uint8_t *dst[], int dstStride[]);

/**
 * Write one line of horizontally scaled data to planar output
 * without any additional vertical scaling (or point-scaling).
 *
 * @param src     scaled source data, 15 bits for 8-10-bit output,
 *                19 bits for 16-bit output (in int32_t)
 * @param dest    pointer to the output plane. For >8-bit
 *                output, this is in uint16_t
 * @param dstW    width of destination in pixels
 * @param dither  ordered dither array of type int16_t and size 8
 * @param offset  Dither offset
 */
typedef void (*yuv2planar1_fn)(const int16_t *src, uint8_t *dest, int dstW,
                               const uint8_t *dither, int offset);

/**
 * Write one line of horizontally scaled data to planar output
 * with multi-point vertical scaling between input pixels.
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
                               const int16_t **src, uint8_t *dest, int dstW,
                               const uint8_t *dither, int offset);

/**
 * Write one line of horizontally scaled chroma to interleaved output
 * with multi-point vertical scaling between input pixels.
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


struct SwsSlice;
struct SwsFilterDescriptor;

/* This struct should be aligned on at least a 32-byte boundary. */
typedef struct SwsContext {

    SwsFunc swscale;
    int srcW;                     ///< Width  of source      luma/alpha planes.
    int srcH;                     ///< Height of source      luma/alpha planes.
    int dstH;                     ///< Height of destination luma/alpha planes.
    int chrSrcW;                  ///< Width  of source      chroma     planes.
    int chrSrcH;                  ///< Height of source      chroma     planes.
    int chrDstW;                  ///< Width  of destination chroma     planes.
    int chrDstH;                  ///< Height of destination chroma     planes.
    int lumXInc, chrXInc;
    int lumYInc, chrYInc;
    enum AVPixelFormat dstFormat; ///< Destination pixel format.
    enum AVPixelFormat srcFormat; ///< Source      pixel format.
    int dstFormatBpp;             ///< Number of bits per pixel of the destination pixel format.
    int srcFormatBpp;             ///< Number of bits per pixel of the source      pixel format.
    int dstBpc, srcBpc;
    int chrSrcHSubSample;         ///< Binary logarithm of horizontal subsampling factor between luma/alpha and chroma planes in source      image.
    int chrSrcVSubSample;         ///< Binary logarithm of vertical   subsampling factor between luma/alpha and chroma planes in source      image.
    int chrDstHSubSample;         ///< Binary logarithm of horizontal subsampling factor between luma/alpha and chroma planes in destination image.
    int chrDstVSubSample;         ///< Binary logarithm of vertical   subsampling factor between luma/alpha and chroma planes in destination image.
    int vChrDrop;                 ///< Binary logarithm of extra vertical subsampling factor in source image chroma planes specified by user.
    int sliceDir;                 ///< Direction that slices are fed to the scaler (1 = top-to-bottom, -1 = bottom-to-top).

    int numDesc;
    int descIndex[2];
    int numSlice;
    struct SwsSlice *slice;
    struct SwsFilterDescriptor *desc;

    /**
     * @name Scaled horizontal lines ring buffer.
     * The horizontal scaler keeps just enough scaled lines in a ring buffer
     * so they may be passed to the vertical scaler. The pointers to the
     * allocated buffers for each line are duplicated in sequence in the ring
     * buffer to simplify indexing and avoid wrapping around between lines
     * inside the vertical scaler code. The wrapping is done before the
     * vertical scaler is called.
     */
    //@{
    int lastInLumBuf;             ///< Last scaled horizontal luma/alpha line from source in the ring buffer.
    int lastInChrBuf;             ///< Last scaled horizontal chroma     line from source in the ring buffer.
    int lumBufIndex;              ///< Index in ring buffer of the last scaled horizontal luma/alpha line from source.
    int chrBufIndex;              ///< Index in ring buffer of the last scaled horizontal chroma     line from source.
    //@}

    int16_t *hLumFilter;          ///< Array of horizontal filter coefficients for luma/alpha planes.
    int16_t *hChrFilter;          ///< Array of horizontal filter coefficients for chroma     planes.
    int16_t *vLumFilter;          ///< Array of vertical   filter coefficients for luma/alpha planes.
    int16_t *vChrFilter;          ///< Array of vertical   filter coefficients for chroma     planes.
    int32_t *hLumFilterPos;       ///< Array of horizontal filter starting positions for each dst[i] for luma/alpha planes.
    int32_t *hChrFilterPos;       ///< Array of horizontal filter starting positions for each dst[i] for chroma     planes.
    int32_t *vLumFilterPos;       ///< Array of vertical   filter starting positions for each dst[i] for luma/alpha planes.
    int32_t *vChrFilterPos;       ///< Array of vertical   filter starting positions for each dst[i] for chroma     planes.
    int hLumFilterSize;           ///< Horizontal filter size for luma/alpha pixels.
    int hChrFilterSize;           ///< Horizontal filter size for chroma     pixels.
    int vLumFilterSize;           ///< Vertical   filter size for luma/alpha pixels.
    int vChrFilterSize;           ///< Vertical   filter size for chroma     pixels.

    int dstY;                     ///< Last destination vertical line output from last slice.
    int flags;                   ///< Flags passed by the user to select scaler algorithm, optimizations, subsampling, etc...


    // 以下四个变量初始值应为 -513
    int src_h_chr_pos;
    int dst_h_chr_pos;
    int src_v_chr_pos;
    int dst_v_chr_pos;

    int dstW;                     ///< Width  of destination luma/alpha planes.

    const uint8_t *chrDither8, *lumDither8;

    /* function pointers for swscale() */
    yuv2planar1_fn yuv2plane1;
    yuv2planarX_fn yuv2planeX;
    yuv2interleavedX_fn yuv2nv12cX;


    /// Unscaled conversion of chroma planes to YV12 for horizontal scaler.
    void (*chrToYV12)(uint8_t *dstU, uint8_t *dstV,
                      const uint8_t *src1, const uint8_t *src2, const uint8_t *src3,
                      int width);

    void (*hyScale)(struct SwsContext *c, int16_t *dst, int dstW,
                    const uint8_t *src, const int16_t *filter,
                    const int32_t *filterPos, int filterSize);
    void (*hcScale)(struct SwsContext *c, int16_t *dst, int dstW,
                    const uint8_t *src, const int16_t *filter,
                    const int32_t *filterPos, int filterSize);

    int needs_hcscale; ///< Set if there are chroma planes to be converted.

} SwsContext;


void ff_sws_init_input_funcs(SwsContext *c);
void ff_sws_init_output_funcs(SwsContext *c,
                              yuv2planar1_fn *yuv2plane1,
                              yuv2planarX_fn *yuv2planeX,
                              yuv2interleavedX_fn *yuv2nv12cX);

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

//以上是新加的

// warp input lines in the form (src + width*i + j) to slice format (line[i][j])
// relative=true means first line src[x][0] otherwise first line is src[x][lum/crh Y]
int ff_init_slice_from_src(SwsSlice * s, uint8_t *src[4], int stride[4], int srcW, int lumY, int lumH, int chrY, int chrH, int relative);

// Initialize scaler filter descriptor chain
int ff_init_filters(SwsContext *c);

// Free all filter data
int ff_free_filters(SwsContext *c);

/*
 function for applying ring buffer logic into slice s
 It checks if the slice can hold more @lum lines, if yes
 do nothing otherwise remove @lum least used lines.
 It applies the same procedure for @chr lines.
*/
int ff_rotate_slice(SwsSlice *s, int lum, int chr);


/// initializes lum horizontal scaling descriptor
int ff_init_desc_hscale(SwsFilterDescriptor *desc, SwsSlice *src, SwsSlice *dst, uint16_t *filter, int * filter_pos, int filter_size, int xInc);

/// initializes chr pixel format conversion descriptor
int ff_init_desc_cfmt_convert(SwsFilterDescriptor *desc, SwsSlice * src, SwsSlice *dst);

/// initializes chr horizontal scaling descriptor
int ff_init_desc_chscale(SwsFilterDescriptor *desc, SwsSlice *src, SwsSlice *dst, uint16_t *filter, int * filter_pos, int filter_size, int xInc);


/// initializes vertical scaling descriptors
int ff_init_vscale(SwsContext *c, SwsFilterDescriptor *desc, SwsSlice *src, SwsSlice *dst);

/// setup vertical scaler functions
void ff_init_vscale_pfn(SwsContext *c, yuv2planar1_fn yuv2plane1, yuv2planarX_fn yuv2planeX,
    yuv2interleavedX_fn yuv2nv12cX);


/**
 * Scale the image slice in srcSlice and put the resulting scaled
 * slice in the image in dst. A slice is a sequence of consecutive
 * rows in an image.
 *
 * Slices have to be provided in sequential order, either in
 * top-bottom or bottom-top order. If slices are provided in
 * non-sequential order the behavior of the function is undefined.
 *
 * @param c         the scaling context previously created with
 *                  sws_getContext()
 * @param srcSlice  the array containing the pointers to the planes of
 *                  the source slice
 * @param srcStride the array containing the strides for each plane of
 *                  the source image
 * @param srcSliceY the position in the source image of the slice to
 *                  process, that is the number (counted starting from
 *                  zero) in the image of the first row of the slice
 * @param srcSliceH the height of the source slice, that is the number
 *                  of rows in the slice
 * @param dst       the array containing the pointers to the planes of
 *                  the destination image
 * @param dstStride the array containing the strides for each plane of
 *                  the destination image
 * @return          the height of the output slice
 */
int sws_scale(struct SwsContext *c, const uint8_t *const srcSlice[],
              const int srcStride[], int srcSliceY, int srcSliceH,
              uint8_t *const dst[], const int dstStride[]);

#endif
/* SWSCALE_SWSCALE_INTERNAL_H */
