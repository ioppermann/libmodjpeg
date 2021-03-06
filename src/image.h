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

#ifndef _LIBMODJPEG_IMAGE_H_
#define _LIBMODJPEG_IMAGE_H_

#include "libmodjpeg.h"

int mj_encode_raw_to_jpeg_memory(unsigned char **memory, size_t *len, unsigned char *rawdata, int colorspace, J_COLOR_SPACE jpeg_colorspace, mj_sampling_t *s, int width, int height);

int mj_decode_jpeg_file_to_raw(unsigned char **rawdata, int *width, int *height, int want_colorspace, const char *filename);
int mj_decode_jpeg_memory_to_raw(unsigned char **rawdata, int *width, int *height, int want_colorspace, const unsigned char *memory, size_t blen);
int mj_decode_jpeg_to_raw(unsigned char **data, int *width, int *height, int want_colorspace, struct jpeg_decompress_struct *cinfo);

int mj_read_file(unsigned char **buffer, size_t *len, const char *filename);

#endif
