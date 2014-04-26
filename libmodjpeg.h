/*
 * libmodjpeg.h
 *
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
#include "jerror.h"

#define MODJPEG_TOP	1
#define MODJPEG_BOTTOM	2
#define MODJPEG_LEFT	4
#define MODJPEG_RIGHT	8

#define MODJPEG_NONE		0
#define MODJPEG_OPTIMIZE	1

#define MODJPEG_RGB	1
#define MODJPEG_RGBA	2
#define MODJPEG_YCbCr	3
#define MODJPEG_YCbCrA	4
#define MODJPEG_Y	5

#define MODJPEG_BLOCKSIZE	DCTSIZE
#define MODJPEG_BLOCKSIZE2	DCTSIZE2

typedef float* modjpeg_image_block;

typedef struct modjpeg_image_component {
	int h_samp_factor;
	int v_samp_factor;

	JDIMENSION height_in_sblocks[2];
	JDIMENSION width_in_sblocks[2];

	JDIMENSION height_in_blocks;
	JDIMENSION width_in_blocks;

	modjpeg_image_block **sblock[2];

	modjpeg_image_block **block;
} modjpeg_image_component;

typedef struct modjpeg_image {
	int max_h_samp_factor;
	int max_v_samp_factor;
	JDIMENSION image_width;
	JDIMENSION image_height;
	int num_components;
	J_COLOR_SPACE jpeg_color_space;

	modjpeg_image_component **component;
} modjpeg_image;

typedef struct modjpeg {
	int clogos;
	modjpeg_image **logos;
	modjpeg_image **masks;

	int cwatermarks;
	modjpeg_image **watermarks;

	float resamp_c[MODJPEG_BLOCKSIZE2], resamp_d[MODJPEG_BLOCKSIZE2], resamp_e[MODJPEG_BLOCKSIZE2], resamp_f[MODJPEG_BLOCKSIZE2];
	float resamp_ct[MODJPEG_BLOCKSIZE2], resamp_dt[MODJPEG_BLOCKSIZE2], resamp_et[MODJPEG_BLOCKSIZE2], resamp_ft[MODJPEG_BLOCKSIZE2];

	float costable[4 * MODJPEG_BLOCKSIZE];
} modjpeg;

typedef struct modjpeg_handle {
	modjpeg *mj;

	struct jpeg_decompress_struct cinfo;
	jvirt_barray_ptr *coef;
} modjpeg_handle;

// Public Prototypes

int modjpeg_init(modjpeg *mj);
int modjpeg_reset(modjpeg *mj);
int modjpeg_destroy(modjpeg *mj);

// Load JPEG image from file
int modjpeg_set_logo_file(modjpeg *mj, const char *logo, const char *mask);
int modjpeg_set_watermark_file(modjpeg *mj, const char *watermark);
modjpeg_handle *modjpeg_set_image_file(modjpeg *mj, const char *image);

// Load JPEG image from memory
int modjpeg_set_logo_mem(modjpeg *mj, const char *logo, int logo_size, const char *mask, int mask_size);
int modjpeg_set_watermark_mem(modjpeg *mj, const char *watermark, int watermark_size);
modjpeg_handle *modjpeg_set_image_mem(modjpeg *mj, const char *image, int image_size);

// Load raw image from memory
int modjpeg_set_logo_raw(modjpeg *mj, const char *logo, int width, int height, int type);
int modjpeg_set_watermark_raw(modjpeg *mj, const char *watermark, int width, int height, int type);

// Load JPEG image from file pointer
modjpeg_handle *modjpeg_set_image_fp(modjpeg *mj, const FILE *image);

// Store resulting image to file or memory
int modjpeg_get_image_file(modjpeg_handle *m, const char *fname, int options);
int modjpeg_get_image_mem(modjpeg_handle *m, char **buffer, int *size, int options);
int modjpeg_get_image_fp(modjpeg_handle *m, const FILE *fp, int options);

// Unload images
int modjpeg_unset_logo(modjpeg *mj, int lid);
int modjpeg_unset_watermark(modjpeg *mj, int wid);
int modjpeg_unset_image(modjpeg_handle *m);

// Apply logo or watermark
int modjpeg_add_logo(modjpeg_handle *m, int lid, int position);
int modjpeg_add_watermark(modjpeg_handle *m, int wid);

// Effects
int modjpeg_effect_grayscale(modjpeg_handle *m);
int modjpeg_effect_pixelate(modjpeg_handle *m);
int modjpeg_effect_tint(modjpeg_handle *m, int cb_value, int cr_value);
int modjpeg_effect_luminance(modjpeg_handle *m, int value);

#endif
