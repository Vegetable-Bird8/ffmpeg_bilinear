#include <stdio.h>
#include <string.h>
#include "pixdesc.h"


static const AVPixFmtDescriptor av_pix_fmt_descriptors[] = {
    [AV_PIX_FMT_YUV420P] = {
        .name = "yuv420p",
        .nb_components = 3,
        .log2_chroma_w = 1,
        .log2_chroma_h = 1,
        .bpc           = 8,
    },
    [AV_PIX_FMT_YUV444P] = {
        .name = "yuv444p",
        .nb_components = 3,
        .log2_chroma_w = 0,
        .log2_chroma_h = 0,
        .bpc           = 8,
    },
    [AV_PIX_FMT_NV12] = {
        .name = "nv12",
        .nb_components = 3,
        .log2_chroma_w = 1,
        .log2_chroma_h = 1,
        .bpc           = 8,
    },
    [AV_PIX_FMT_NV21] = {
        .name = "nv21",
        .nb_components = 3,
        .log2_chroma_w = 1,
        .log2_chroma_h = 1,
        .bpc           = 8,
    }
};


int av_get_bits_per_pixel(const AVPixFmtDescriptor *pixdesc)
{
    int c, bits = 0;
    int log2_pixels = pixdesc->log2_chroma_w + pixdesc->log2_chroma_h;

    for (c = 0; c < pixdesc->nb_components; c++) {
        int s = c == 1 || c == 2 ? 0 : log2_pixels;
        bits += pixdesc->bpc << s;
    }

    return bits >> log2_pixels;
}


const AVPixFmtDescriptor *av_pix_fmt_desc_get(enum AVPixelFormat pix_fmt)
{
    if (pix_fmt < 0 || pix_fmt >= AV_PIX_FMT_NB)
        return NULL;
    return &av_pix_fmt_descriptors[pix_fmt];
}