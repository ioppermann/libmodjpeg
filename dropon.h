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

#ifndef _LIBMODJPEG_DROPON_H_
#define _LIBMODJPEG_DROPON_H_

#ifdef WITH_LIBPNG
	#include <png.h>
#endif

#include "libmodjpeg.h"

int mj_read_droponimage_from_memory(mj_compileddropon_t *cd, const char *memory, size_t len);
int mj_read_droponalpha_from_memory(mj_compileddropon_t *cd, const char *memory, size_t len);

int mj_compile_dropon(mj_compileddropon_t *cd, mj_dropon_t *d, J_COLOR_SPACE colorspace, mj_sampling_t *s, int blockoffset_x, int blockoffset_y, int crop_x, int crop_y, int crop_w, int crop_h);

void mj_free_compileddropon(mj_compileddropon_t *cd);
void mj_free_component(mj_component_t *c);

#ifdef WITH_LIBPNG
int mj_read_dropon_from_png(mj_dropon_t *d, png_image *image);
#endif

#endif