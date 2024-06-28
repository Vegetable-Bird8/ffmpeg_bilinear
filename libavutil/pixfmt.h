#ifndef AVUTIL_PIXFMT_H
#define AVUTIL_PIXFMT_H

#define AVPALETTE_SIZE 1024
#define AVPALETTE_COUNT 256

enum AVPixelFormat {
    AV_PIX_FMT_NONE = -1,
    AV_PIX_FMT_YUV420P = 0,   ///< planar YUV 4:2:0, 12bpp, (1 Cr & Cb sample per 2x2 Y samples)
    AV_PIX_FMT_YUV444P = 1,   ///< planar YUV 4:4:4, 24bpp, (1 Cr & Cb sample per 1x1 Y samples)
    AV_PIX_FMT_NV12 = 2,      ///< planar YUV 4:2:0, 12bpp, 1 plane for Y and 1 plane for the UV components, which are interleaved (first byte U and the following byte V)
    AV_PIX_FMT_NV21 = 3,      ///< as above, but U and V bytes are swapped
    AV_PIX_FMT_NB         ///< number of pixel formats, DO NOT USE THIS if you want to link with shared libav* because the number of formats might differ between versions
};


/**
 * MPEG vs JPEG YUV range.
 */
enum AVColorRange {
    AVCOL_RANGE_UNSPECIFIED = 0,
    AVCOL_RANGE_MPEG        = 1, ///< the normal 219*2^(n-8) "MPEG" YUV ranges
    AVCOL_RANGE_JPEG        = 2, ///< the normal     2^n-1   "JPEG" YUV ranges
    AVCOL_RANGE_NB               ///< Not part of ABI
};


#endif /* AVUTIL_PIXFMT_H */
