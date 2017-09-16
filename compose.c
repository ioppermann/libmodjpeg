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

#include <stdio.h>
#include <string.h>

#include "libmodjpeg.h"
#include "dropon.h"
#include "convolve.h"
#include "compose.h"

int mj_compose(mj_jpeg_t *m, mj_dropon_t *d, unsigned int align, int x_offset, int y_offset) {
	printf("entering %s\n", __FUNCTION__);

	int reload = 0;
	int h_offset = 0, v_offset = 0, offset = 0;
	jpeg_component_info *component_m = NULL;

	if(m == NULL || d == NULL) {
		return 0;
	}

	printf("blend: %d\n", d->blend);

	if(d->blend == MJ_BLEND_NONE) {
		return 0;
	}

	if(d->image != NULL) {
		// is the colorspace the same?
		if(m->cinfo.jpeg_color_space != d->image_colorspace) {
			reload = 1;
		}

		// is the sampling the same?
		int c = 0;
		for(c = 0; c < m->cinfo.num_components; c++) {
			component_m = &m->cinfo.comp_info[c];

			if(component_m->h_samp_factor != d->image[c].h_samp_factor) {
				reload = 1;
			}

			if(component_m->v_samp_factor != d->image[c].v_samp_factor) {
				reload = 1;
			}
		}
	}
	else {
		reload = 1;
	}

	// is the offset the same?
	// calculate needed offset, compare with d->offset

	if((align & MJ_ALIGN_LEFT) != 0) {
		h_offset = 0;
	}
	else if((align & MJ_ALIGN_RIGHT) != 0) {
		h_offset = m->cinfo.output_width - d->raw_width;
	}
	else {
		h_offset = m->cinfo.output_width / 2 - d->raw_width / 2;
	}

	h_offset += x_offset;

	offset = h_offset % m->sampling.h_factor;
	h_offset = h_offset / m->sampling.h_factor;

	if((align & MJ_ALIGN_TOP) != 0) {
		v_offset = 0;
	}
	else if((align & MJ_ALIGN_BOTTOM) != 0) {
		v_offset = m->cinfo.output_height - d->raw_height;
	}
	else {
		v_offset = m->cinfo.output_height / 2 - d->raw_height / 2;
	}

	v_offset += y_offset;

	offset += m->sampling.h_factor * (v_offset % m->sampling.v_factor);
	v_offset = v_offset / m->sampling.v_factor;

	if(offset != d->offset) {
		reload = 1;
	}

	if(reload == 1) {
		printf("reloading dropon\n");

		mj_update_dropon(d, m->cinfo.jpeg_color_space, &m->sampling, offset);
	}

	if(d->blend == MJ_BLEND_FULL && d->offset == 0) {
		mj_compose_without_mask(m, d, h_offset, v_offset);
	}
	else {
		mj_compose_with_mask(m, d, h_offset, v_offset);
	}

	return 0;
}

int mj_compose_without_mask(mj_jpeg_t *m, mj_dropon_t *d, int h_offset, int v_offset) {
	printf("entering %s\n", __FUNCTION__);

	int c, k, l, i;
	int width_offset = 0, height_offset = 0;
	int width_in_blocks = 0, height_in_blocks = 0;
	struct jpeg_decompress_struct *cinfo_m;
	jpeg_component_info *component_m;
	JBLOCKARRAY blocks_m;
	JCOEFPTR coefs_m;

	mj_component_t *imagecomp;
	mj_block_t *imageblock;

	cinfo_m = &m->cinfo;

	for(c = 0; c < d->image_ncomponents; c++) {
		printf("component %d\n", c);

		component_m = &cinfo_m->comp_info[c];
		imagecomp = &d->image[c];

		width_in_blocks = imagecomp->width_in_blocks;
		height_in_blocks = imagecomp->height_in_blocks;

		width_offset = h_offset * component_m->h_samp_factor;
		height_offset = v_offset * component_m->v_samp_factor;

		/* Die Werte des Logos in das Bild kopieren */
		for(l = 0; l < height_in_blocks; l++) {
			blocks_m = (*cinfo_m->mem->access_virt_barray)((j_common_ptr)&cinfo_m, m->coef[c], height_offset + l, 1, TRUE);

			for(k = 0; k < width_in_blocks; k++) {
				coefs_m = blocks_m[0][width_offset + k];
				imageblock = imagecomp->blocks[height_in_blocks * l + k];

				for(i = 0; i < DCTSIZE2; i += 8) {
					coefs_m[i + 0] = (int)imageblock[i + 0];
					coefs_m[i + 1] = (int)imageblock[i + 1];
					coefs_m[i + 2] = (int)imageblock[i + 2];
					coefs_m[i + 3] = (int)imageblock[i + 3];
					coefs_m[i + 4] = (int)imageblock[i + 4];
					coefs_m[i + 5] = (int)imageblock[i + 5];
					coefs_m[i + 6] = (int)imageblock[i + 6];
					coefs_m[i + 7] = (int)imageblock[i + 7];
				}
			}
		}
	}

	return 0;
}

int mj_compose_with_mask(mj_jpeg_t *m, mj_dropon_t *d, int h_offset, int v_offset) {
	printf("entering %s\n", __FUNCTION__);

	int c, k, l, i;
	int width_offset = 0, height_offset = 0;
	int width_in_blocks = 0, height_in_blocks = 0;
	struct jpeg_decompress_struct *cinfo_m;
	jpeg_component_info *component_m;
	JBLOCKARRAY blocks_m;
	JCOEFPTR coefs_m;
	float X[DCTSIZE2], Y[DCTSIZE2];

	mj_component_t *imagecomp, *alphacomp;
	mj_block_t *imageblock, *alphablock;

	cinfo_m = &m->cinfo;

	for(c = 0; c < d->image_ncomponents; c++) {
		component_m = &cinfo_m->comp_info[c];
		imagecomp = &d->image[c];
		alphacomp = &d->alpha[c];

		width_in_blocks = imagecomp->width_in_blocks;
		height_in_blocks = imagecomp->height_in_blocks;

		width_offset = h_offset * component_m->h_samp_factor;
		height_offset = v_offset * component_m->v_samp_factor;

		/* Die Werte des Logos in das Bild kopieren */
		for(l = 0; l < height_in_blocks; l++) {
			blocks_m = (*cinfo_m->mem->access_virt_barray)((j_common_ptr)&cinfo_m, m->coef[c], height_offset + l, 1, TRUE);

			for(k = 0; k < width_in_blocks; k++) {
				coefs_m = blocks_m[0][width_offset + k];
				alphablock = alphacomp->blocks[height_in_blocks * l + k];
				imageblock = imagecomp->blocks[height_in_blocks * l + k];
/*
				int p, q;

				for(p = 0; p < DCTSIZE; p++) {
					for(q = 0; q < DCTSIZE; q++) {
						printf("%.2f ", imageblock[DCTSIZE * p + q]);
					}
					printf("\n");
				}
*/
				// x = x0 - x1
				printf("component %d (%d,%d): x0 - x1 | ", c, l, k);
				for(i = 0; i < DCTSIZE2; i += 8) {
					X[i + 0] = imageblock[i + 0] - coefs_m[i + 0];
					X[i + 1] = imageblock[i + 1] - coefs_m[i + 1];
					X[i + 2] = imageblock[i + 2] - coefs_m[i + 2];
					X[i + 3] = imageblock[i + 3] - coefs_m[i + 3];
					X[i + 4] = imageblock[i + 4] - coefs_m[i + 4];
					X[i + 5] = imageblock[i + 5] - coefs_m[i + 5];
					X[i + 6] = imageblock[i + 6] - coefs_m[i + 6];
					X[i + 7] = imageblock[i + 7] - coefs_m[i + 7];
				}

				memset(Y, 0, DCTSIZE2 * sizeof(float));

				// y' = w * x (Faltung)
				printf("w * x | ");
				for(i = 0; i < DCTSIZE; i++) {
					mj_convolve(X, Y, alphablock[(i * DCTSIZE) + 0], i, 0);
					mj_convolve(X, Y, alphablock[(i * DCTSIZE) + 1], i, 1);
					mj_convolve(X, Y, alphablock[(i * DCTSIZE) + 2], i, 2);
					mj_convolve(X, Y, alphablock[(i * DCTSIZE) + 3], i, 3);
					mj_convolve(X, Y, alphablock[(i * DCTSIZE) + 4], i, 4);
					mj_convolve(X, Y, alphablock[(i * DCTSIZE) + 5], i, 5);
					mj_convolve(X, Y, alphablock[(i * DCTSIZE) + 6], i, 6);
					mj_convolve(X, Y, alphablock[(i * DCTSIZE) + 7], i, 7);
				}

				// y = x1 + y'
				printf("x1 + y'\n");
				for(i = 0; i < DCTSIZE2; i += 8) {
					coefs_m[i + 0] += (int)Y[i + 0];
					coefs_m[i + 1] += (int)Y[i + 1];
					coefs_m[i + 2] += (int)Y[i + 2];
					coefs_m[i + 3] += (int)Y[i + 3];
					coefs_m[i + 4] += (int)Y[i + 4];
					coefs_m[i + 5] += (int)Y[i + 5];
					coefs_m[i + 6] += (int)Y[i + 6];
					coefs_m[i + 7] += (int)Y[i + 7];
				}
			}
		}
	}

	return 0;
}

