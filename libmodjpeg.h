/*
 * Copyright (c) 2006, Ingo Oppermann
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the copyright holder nor the names of its
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * -----------------------------------------------------------------------------
 *
 */

#ifndef _LIBMODJPEG_H_
#define _LIBMODJPEG_H_

#include <stdio.h>

#include "jpeglib.h"

#define MJ_COLORSPACE_RGBA		1
#define MJ_COLORSPACE_RGB		2
#define MJ_COLORSPACE_GRAYSCALE 	3
#define MJ_COLORSPACE_GRAYSCALEA	4
#define MJ_COLORSPACE_YCC		5
#define MJ_COLORSPACE_YCCA		6

#define MJ_ALIGN_LEFT			(1 << 0)
#define MJ_ALIGN_RIGHT			(1 << 1)
#define MJ_ALIGN_TOP			(1 << 2)
#define MJ_ALIGN_BOTTOM			(1 << 3)
#define MJ_ALIGN_CENTER			(1 << 4)

#define MJ_BLEND_NONUNIFORM		-1
#define MJ_BLEND_NONE			0
#define MJ_BLEND_FULL			255

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

	int nblocks;
	mj_block_t **blocks;
} mj_component_t;

typedef struct {
	struct jpeg_decompress_struct cinfo;
	jvirt_barray_ptr *coef;

	int h_blocks;
	int v_blocks;

	mj_sampling_t sampling;
} mj_jpeg_t;

typedef struct {
	char *raw_image;
	char *raw_alpha;

	size_t raw_width;
	size_t raw_height;
	int raw_colorspace;

	int blend;
	unsigned short offset;

	int image_ncomponents;
	int image_colorspace;

	mj_component_t *image;

	int alpha_ncomponents;
	unsigned short alpha_offset;

	mj_component_t *alpha;
} mj_dropon_t;

mj_dropon_t *mj_read_dropon_from_buffer(const char *rawdata, unsigned int colorspace, size_t width, size_t height, short blend);
mj_dropon_t *mj_read_dropon_from_jpeg(const char *filename, const char *mask, short blend);

mj_jpeg_t *mj_read_jpeg_from_buffer(const char *buffer, size_t len);
mj_jpeg_t *mj_read_jpeg_from_file(const char *filename);

int mj_compose(mj_jpeg_t *m, mj_dropon_t *d, unsigned int align, int x_offset, int y_offset);

int mj_write_jpeg_to_buffer(mj_jpeg_t *m, char **buffer, size_t *len);
int mj_write_jpeg_to_file(mj_jpeg_t *m, char *filename);

void mj_destroy_jpeg(mj_jpeg_t *m);
void mj_destroy_dropon(mj_dropon_t *d);

int mj_effect_grayscale(mj_jpeg_t *m);
int mj_effect_pixelate(mj_jpeg_t *m);
int mj_effect_tint(mj_jpeg_t *m, int cb_value, int cr_value);
int mj_effect_luminance(mj_jpeg_t *m, int value);

#endif
