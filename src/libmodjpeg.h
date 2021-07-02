/*
 * Copyright (c) 2006+ Ingo Oppermann
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef _LIBMODJPEG_H_
#define _LIBMODJPEG_H_

// clang-format off
// The stdio.h header must be before jpeglib.h, otherwise the compiler
// complains about a missing definition of size_t
#include <stdio.h>
#include <jpeglib.h>
// clang-format on

#define MJ_LIB_VERSION_MAJOR   1
#define MJ_LIB_VERSION_MINOR   0
#define MJ_LIB_VERSION_RELEASE 0
#define MJ_LIB_VERSION         10000

#define MJ_COLORSPACE_RGB        1
#define MJ_COLORSPACE_RGBA       2
#define MJ_COLORSPACE_GRAYSCALE  3
#define MJ_COLORSPACE_GRAYSCALEA 4
#define MJ_COLORSPACE_YCC        5
#define MJ_COLORSPACE_YCCA       6

#define MJ_ALIGN_LEFT   (1 << 0)
#define MJ_ALIGN_RIGHT  (1 << 1)
#define MJ_ALIGN_TOP    (1 << 2)
#define MJ_ALIGN_BOTTOM (1 << 3)
#define MJ_ALIGN_CENTER (1 << 4)

#define MJ_BLEND_NONUNIFORM -1
#define MJ_BLEND_NONE       0
#define MJ_BLEND_FULL       255

#define MJ_OPTION_NONE        0
#define MJ_OPTION_OPTIMIZE    (1 << 0)
#define MJ_OPTION_PROGRESSIVE (1 << 1)
#define MJ_OPTION_ARITHMETRIC (1 << 2)

#define MJ_OK                         0
#define MJ_ERR_MEMORY                 1
#define MJ_ERR_NULL_DATA              2
#define MJ_ERR_DROPON_DIMENSIONS      3
#define MJ_ERR_UNSUPPORTED_COLORSPACE 4
#define MJ_ERR_DECODE_JPEG            5
#define MJ_ERR_ENCODE_JPEG            6
#define MJ_ERR_FILEIO                 7
#define MJ_ERR_IMAGE_SIZE             8
#define MJ_ERR_UNSUPPORTED_FILETYPE   9

typedef struct {
    int h_samp_factor;
    int v_samp_factor;
} mj_samplingfactor_t;

typedef struct {
    int max_h_samp_factor;
    int max_v_samp_factor;

    int h_factor;
    int v_factor;

    mj_samplingfactor_t samp_factor[4];
} mj_sampling_t;

typedef float mj_block_t;

typedef struct {
    int width_in_blocks;
    int height_in_blocks;

    int h_samp_factor;
    int v_samp_factor;

    int          nblocks;
    mj_block_t **blocks;
} mj_component_t;

typedef struct {
    struct jpeg_decompress_struct cinfo;
    jvirt_barray_ptr *            coef;

    int width;
    int height;

    mj_sampling_t sampling;
} mj_jpeg_t;

typedef struct {
    unsigned char *image;
    unsigned char *alpha;

    int width;
    int height;
    int colorspace;

    int blend;
} mj_dropon_t;

typedef struct {
    int             image_ncomponents;
    int             image_colorspace;
    mj_component_t *image;

    int             alpha_ncomponents;
    mj_component_t *alpha;
} mj_compileddropon_t;

void mj_init_dropon(mj_dropon_t *d);
int  mj_read_dropon_from_raw(mj_dropon_t *d, const unsigned char *rawdata, unsigned int colorspace, int width, int height, short blend);
int  mj_read_dropon_from_memory(mj_dropon_t *d, const unsigned char *memory, size_t len, const unsigned char *maskmemory, size_t masklen, short blend);
int  mj_read_dropon_from_file(mj_dropon_t *d, const char *filename, const char *maskfilename, short blend);

void mj_init_jpeg(mj_jpeg_t *m);
int  mj_read_jpeg_from_memory(mj_jpeg_t *m, const unsigned char *memory, size_t len, size_t max_pixel);
int  mj_read_jpeg_from_file(mj_jpeg_t *m, const char *filename, size_t max_pixel);

int mj_compose(mj_jpeg_t *m, mj_dropon_t *d, unsigned int align, int offset_x, int offset_y);

int mj_write_jpeg_to_memory(mj_jpeg_t *m, unsigned char **memory, size_t *len, int options);
int mj_write_jpeg_to_file(mj_jpeg_t *m, char *filename, int options);

void mj_free_jpeg(mj_jpeg_t *m);
void mj_free_dropon(mj_dropon_t *d);

int mj_effect_grayscale(mj_jpeg_t *m);
int mj_effect_pixelate(mj_jpeg_t *m);
int mj_effect_tint(mj_jpeg_t *m, int cb_value, int cr_value);
int mj_effect_luminance(mj_jpeg_t *m, int value);

#endif
