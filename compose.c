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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "libmodjpeg.h"
#include "dropon.h"
#include "convolve.h"
#include "compose.h"

int mj_compose(mj_jpeg_t *m, mj_dropon_t *d, unsigned int align, int x_offset, int y_offset) {
	fprintf(stderr, "entering %s\n", __FUNCTION__);

	if(m == NULL || d == NULL) {
		return MJ_ERR;
	}

	fprintf(stderr, "(x_offset, y_offset) = (%d, %d)\n", x_offset, y_offset);

	//fprintf(stderr, "blend: %d\n", d->blend);

	if(d->blend == MJ_BLEND_NONE) {
		return MJ_OK;
	}

	// calculate crop of dropon

	int h_offset = 0, v_offset = 0;
	int crop_x = 0, crop_y = 0, crop_w = 0, crop_h = 0;

	// horizontally

	if((align & MJ_ALIGN_LEFT) != 0) {
		h_offset = 0;
	}
	else if((align & MJ_ALIGN_RIGHT) != 0) {
		h_offset = m->width - d->width;
	}
	else {
		h_offset = m->width / 2 - d->width / 2;
	}

	h_offset += x_offset;

	crop_x = 0;

	if(h_offset < 0) {
		crop_x = -h_offset;
	}

	crop_w = d->width - crop_x;

	if(crop_x > d->width) {				// dropon shifted more off the left border than its width
		crop_w = 0;
	}
	else if(h_offset > m->width) {				// dropon shifted off the right border
		crop_w = 0;
	}
	else if(h_offset + crop_x + crop_w > m->width) {	// dropon partially shifted off the right border
		crop_w = m->width - crop_x - h_offset;
	}

	// vertically

	if((align & MJ_ALIGN_TOP) != 0) {
		v_offset = 0;
	}
	else if((align & MJ_ALIGN_BOTTOM) != 0) {
		v_offset = m->height - d->height;
	}
	else {
		v_offset = m->height / 2 - d->height / 2;
	}

	v_offset += y_offset;

	crop_y = 0;

	if(v_offset < 0) {
		crop_y = -v_offset;
	}

	crop_h = d->height - crop_y;

	if(crop_y > d->height) {
		crop_h = 0;
	}
	else if(v_offset > m->height) {
		crop_h = 0;
	}
	else if(v_offset + crop_y + crop_h > m->height) {
		crop_h = m->height - crop_y - v_offset;
	}

	fprintf(stderr, "crop (%d, %d, %d, %d)\n", crop_x, crop_y, crop_w, crop_h);

	if(crop_w == 0 || crop_h == 0) {
		return MJ_OK;
	}

	int block_x = h_offset % m->sampling.h_factor;
	if(block_x < 0) {
		block_x = 0;
	}
	int block_y = v_offset % m->sampling.v_factor;
	if(block_y < 0) {
		block_y = 0;
	}

	fprintf(stderr, "block offset (%d, %d)\n", block_x, block_y);
	
	fprintf(stderr, "compiling dropon\n");

	mj_compileddropon_t cd;

	int rv = mj_compile_dropon(&cd, d, m->cinfo.jpeg_color_space, &m->sampling, block_x, block_y, crop_x, crop_y, crop_w, crop_h);
	if(rv != MJ_OK) {
		return rv;
	}

	h_offset /= m->sampling.h_factor;
	v_offset /= m->sampling.v_factor;

	if(h_offset < 0) {
		h_offset = 0;
	}

	if(v_offset < 0) {
		v_offset = 0;
	}

	fprintf(stderr, "offset block (%d, %d)\n", h_offset, v_offset);

	mj_compose_with_mask(m, &cd, h_offset, v_offset);

	mj_free_compileddropon(&cd);

	return MJ_OK;
}

void mj_compose_without_mask(mj_jpeg_t *m, mj_compileddropon_t *cd, int h_offset, int v_offset) {
	fprintf(stderr, "entering %s\n", __FUNCTION__);

	if(m == NULL || cd == NULL) {
		return;
	}

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

	for(c = 0; c < cd->image_ncomponents; c++) {
		component_m = &cinfo_m->comp_info[c];
		imagecomp = &cd->image[c];

		width_in_blocks = imagecomp->width_in_blocks;
		height_in_blocks = imagecomp->height_in_blocks;

		width_offset = h_offset * component_m->h_samp_factor;
		height_offset = v_offset * component_m->v_samp_factor;

		//fprintf(stderr, "*component %d (%d,%d) %p\n", c, width_in_blocks, height_in_blocks, imagecomp->blocks);

		// copy the values from the dropon into the image
		for(l = 0; l < height_in_blocks; l++) {
			blocks_m = (*cinfo_m->mem->access_virt_barray)((j_common_ptr)&cinfo_m, m->coef[c], height_offset + l, 1, TRUE);

			for(k = 0; k < width_in_blocks; k++) {
				coefs_m = blocks_m[0][width_offset + k];
				imageblock = imagecomp->blocks[width_in_blocks * l + k];

				//fprintf(stderr, "*component (%d,%d) %p\n", l, k, imageblock);

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

		break;
	}

	return;
}

void mj_compose_with_mask(mj_jpeg_t *m, mj_compileddropon_t *cd, int h_offset, int v_offset) {
	fprintf(stderr, "entering %s\n", __FUNCTION__);

	if(m == NULL || cd == NULL) {
		return;
	}

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

	for(c = 0; c < cd->image_ncomponents; c++) {
		component_m = &cinfo_m->comp_info[c];
		imagecomp = &cd->image[c];
		alphacomp = &cd->alpha[c];

		width_in_blocks = imagecomp->width_in_blocks;
		height_in_blocks = imagecomp->height_in_blocks;

		width_offset = h_offset * component_m->h_samp_factor;
		height_offset = v_offset * component_m->v_samp_factor;

		fprintf(stderr, "component %d: (%d, %d) %d %d\n", c, width_in_blocks, height_in_blocks, width_offset, height_offset);

		// blend the values from the dropon with the image
		for(l = 0; l < height_in_blocks; l++) {
			blocks_m = (*cinfo_m->mem->access_virt_barray)((j_common_ptr)&cinfo_m, m->coef[c], height_offset + l, 1, TRUE);

			for(k = 0; k < width_in_blocks; k++) {
				coefs_m = blocks_m[0][width_offset + k];
				imageblock = imagecomp->blocks[width_in_blocks * l + k];
				alphablock = alphacomp->blocks[width_in_blocks * l + k];
/*
				int p, q;

				for(p = 0; p < DCTSIZE; p++) {
					for(q = 0; q < DCTSIZE; q++) {
						fprintf(stderr, "%.2f ", imageblock[DCTSIZE * p + q]);
					}
					fprintf(stderr, "\n");
				}
*/
				// x = x0 - x1
				//fprintf(stderr, "component %d (%d,%d): x0 - x1 | ", c, l, k);
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
				//fprintf(stderr, "w * x | ");
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
				//fprintf(stderr, "x1 + y'\n");
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

	return;
}

