# ffmpeg_bilinear
ffmpeg's bilinear isolated,make it simple.
Error Table
AVERROR(ENOMEM) = -12 内存分配失败
AVERROR(EINVAL) = -22 on overflow

explanation:
    /**
     * Scale one horizontal line of input data using a filter over the input
     * lines, to produce one (differently sized) line of output data.
     *
     * @param dst        pointer to destination buffer for horizontally scaled
     *                   data. If the number of bits per component of one
     *                   destination pixel (SwsContext->dstBpc) is <= 10, data
     *                   will be 15 bpc in 16 bits (int16_t) width. Else (i.e.
     *                   SwsContext->dstBpc == 16), data will be 19bpc in
     *                   32 bits (int32_t) width.
     * @param dstW       width of destination image
     * @param src        pointer to source data to be scaled. If the number of
     *                   bits per component of a source pixel (SwsContext->srcBpc)
     *                   is 8, this is 8bpc in 8 bits (uint8_t) width. Else
     *                   (i.e. SwsContext->dstBpc > 8), this is native depth
     *                   in 16 bits (uint16_t) width. In other words, for 9-bit
     *                   YUV input, this is 9bpc, for 10-bit YUV input, this is
     *                   10bpc, and for 16-bit RGB or YUV, this is 16bpc.
     * @param filter     filter coefficients to be used per output pixel for
     *                   scaling. This contains 14bpp filtering coefficients.
     *                   Guaranteed to contain dstW * filterSize entries.
     * @param filterPos  position of the first input pixel to be used for
     *                   each output pixel during scaling. Guaranteed to
     *                   contain dstW entries.
     * @param filterSize the number of input coefficients to be used (and
     *                   thus the number of input pixels to be used) for
     *                   creating a single output pixel. Is aligned to 4
     *                   (and input coefficients thus padded with zeroes)
     *                   to simplify creating SIMD code.
     */
    /** @ */
    void (*hyScale)(struct SwsContext *c, int16_t *dst, int dstW,
                    const uint8_t *src, const int16_t *filter,
                    const int32_t *filterPos, int filterSize);
    void (*hcScale)(struct SwsContext *c, int16_t *dst, int dstW,
                    const uint8_t *src, const int16_t *filter,
                    const int32_t *filterPos, int filterSize);


    /**
     * @name Horizontal and vertical filters.
     * To better understand the following fields, here is a pseudo-code of
     * their usage in filtering a horizontal line:
     * @code
     * for (i = 0; i < width; i++) {
     *     dst[i] = 0;
     *     for (j = 0; j < filterSize; j++)
     *         dst[i] += src[ filterPos[i] + j ] * filter[ filterSize * i + j ];
     *     dst[i] >>= FRAC_BITS; // The actual implementation is fixed-point.
     * }
     * @endcode
     */
    //@
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

make clean
make

