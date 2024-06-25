/*
 * pixel format descriptor
 * Copyright (c) 2009 Michael Niedermayer <michaelni@gmx.at>
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

#ifndef AVUTIL_PIXDESC_H
#define AVUTIL_PIXDESC_H

#include <inttypes.h>
#include "pixfmt.h"

typedef struct AVComponentDescriptor {
    /**
     * Which of the 4 planes contains the component.
     */
    int plane;

    /**
     * Number of elements between 2 horizontally consecutive pixels.
     * Elements are bits for bitstream formats, bytes otherwise.
     */
    int step;

    /**
     * Number of elements before the component of the first pixel.
     * Elements are bits for bitstream formats, bytes otherwise.
     */
    int offset;

    /**
     * Number of least significant bits that must be shifted away
     * to get the value.
     */
    int shift;

    /**
     * Number of bits in the component.
     */
    int depth;

} AVComponentDescriptor;

/**
 * Descriptor that unambiguously describes how the bits of a pixel are
 * stored in the up to 4 data planes of an image. It also stores the
 * subsampling factors and number of components.
 *
 * @note This is separate of the colorspace (RGB, YCbCr, YPbPr, JPEG-style YUV
 *       and all the YUV variants) AVPixFmtDescriptor just stores how values
 *       are stored not what these values represent.
 */
typedef struct AVPixFmtDescriptor {
    const char *name;
    uint8_t nb_components;  ///< The number of components each pixel has, (1-4)

    /**
     * Amount to shift the luma width right to find the chroma width.
     * For YV12 this is 1 for example.
     * chroma_width = AV_CEIL_RSHIFT(luma_width, log2_chroma_w)
     * The note above is needed to ensure rounding up.
     * This value only refers to the chroma components.
     */
    uint8_t log2_chroma_w;

    /**
     * Amount to shift the luma height right to find the chroma height.
     * For YV12 this is 1 for example.
     * chroma_height= AV_CEIL_RSHIFT(luma_height, log2_chroma_h)
     * The note above is needed to ensure rounding up.
     * This value only refers to the chroma components.
     */
    uint8_t log2_chroma_h;

    /**
     * Parameters that describe how pixels are packed.
     * If the format has 1 or 2 components, then luma is 0.
     * If the format has 3 or 4 components:
     *   if the RGB flag is set then 0 is red, 1 is green and 2 is blue;
     *   otherwise 0 is luma, 1 is chroma-U and 2 is chroma-V.
     *
     * If present, the Alpha channel is always the last component.
     */
    AVComponentDescriptor comp[4];

    /**
     * Combination of AV_PIX_FMT_FLAG_... flags.
     */
    uint64_t flags;
    /**
     * Alternative comma-separated names.
     */
    const char *alias;
} AVPixFmtDescriptor;

/**
 * Pixel format is big-endian.
 */
#define AV_PIX_FMT_FLAG_BE           (1 << 0)
/**
 * Pixel format has a palette in data[1], values are indexes in this palette.
 */
#define AV_PIX_FMT_FLAG_PAL          (1 << 1)
/**
 * All values of a component are bit-wise packed end to end.
 */
#define AV_PIX_FMT_FLAG_BITSTREAM    (1 << 2)
/**
 * Pixel format is an HW accelerated format.
 */
#define AV_PIX_FMT_FLAG_HWACCEL      (1 << 3)
/**
 * At least one pixel component is not in the first data plane.
 */
#define AV_PIX_FMT_FLAG_PLANAR       (1 << 4)
/**
 * The pixel format contains RGB-like data (as opposed to YUV/grayscale).
 */
#define AV_PIX_FMT_FLAG_RGB          (1 << 5)

/**
 * The pixel format is "pseudo-paletted". This means that it contains a
 * fixed palette in the 2nd plane but the palette is fixed/constant for each
 * PIX_FMT. This allows interpreting the data as if it was PAL8, which can
 * in some cases be simpler. Or the data can be interpreted purely based on
 * the pixel format without using the palette.
 * An example of a pseudo-paletted format is AV_PIX_FMT_GRAY8
 *
 * @deprecated This flag is deprecated, and will be removed. When it is removed,
 * the extra palette allocation in AVFrame.data[1] is removed as well. Only
 * actual paletted formats (as indicated by AV_PIX_FMT_FLAG_PAL) will have a
 * palette. Starting with FFmpeg versions which have this flag deprecated, the
 * extra "pseudo" palette is already ignored, and API users are not required to
 * allocate a palette for AV_PIX_FMT_FLAG_PSEUDOPAL formats (it was required
 * before the deprecation, though).
 */
#define AV_PIX_FMT_FLAG_PSEUDOPAL    (1 << 6)

/**
 * The pixel format has an alpha channel. This is set on all formats that
 * support alpha in some way, including AV_PIX_FMT_PAL8. The alpha is always
 * straight, never pre-multiplied.
 *
 * If a codec or a filter does not support alpha, it should set all alpha to
 * opaque, or use the equivalent pixel formats without alpha component, e.g.
 * AV_PIX_FMT_RGB0 (or AV_PIX_FMT_RGB24 etc.) instead of AV_PIX_FMT_RGBA.
 */
#define AV_PIX_FMT_FLAG_ALPHA        (1 << 7)

/**
 * The pixel format is following a Bayer pattern
 */
#define AV_PIX_FMT_FLAG_BAYER        (1 << 8)

/**
 * The pixel format contains IEEE-754 floating point values. Precision (double,
 * single, or half) should be determined by the pixel size (64, 32, or 16 bits).
 */
#define AV_PIX_FMT_FLAG_FLOAT        (1 << 9)
int av_get_bits_per_pixel(const AVPixFmtDescriptor *pixdesc);


/**
 * @return a pixel format descriptor for provided pixel format or NULL if
 * this pixel format is unknown.
 */
const AVPixFmtDescriptor *av_pix_fmt_desc_get(enum AVPixelFormat pix_fmt);


/**
 * Utility function to access log2_chroma_w log2_chroma_h from
 * the pixel format AVPixFmtDescriptor.
 *
 * @param[in]  pix_fmt the pixel format
 * @param[out] h_shift store log2_chroma_w (horizontal/width shift)
 * @param[out] v_shift store log2_chroma_h (vertical/height shift)
 *
 * @return 0 on success, AVERROR(ENOSYS) on invalid or unknown pixel format
 */
int av_pix_fmt_get_chroma_sub_sample(enum AVPixelFormat pix_fmt,
                                     int *h_shift, int *v_shift);

#endif /* AVUTIL_PIXDESC_H */
