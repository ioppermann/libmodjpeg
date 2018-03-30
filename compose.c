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

int mj_compose(mj_jpeg_t *m, mj_dropon_t *d, unsigned int align, int offset_x, int offset_y) {
	if(m == NULL || d == NULL) {
		return MJ_ERR_NULL_DATA;
	}

	if(d->blend == MJ_BLEND_NONE) {
		return MJ_OK;
	}

	// depending on the alignment and the offset, we have find out
	// how much of the dropon will be visible.

	// position is the position of the top-left corner of the dropon on the image
	int position_x = 0, position_y = 0;

	// top-left corner of the crop area of the dropon and width and height of the crop area
	// initially the whole dropon
	int crop_x = 0, crop_y = 0, crop_w = d->width, crop_h = d->height;


	// first we have to calculate the position of the dropon on the image,
	// then we know how we have to crop the dropon. in most cases the
	// dropon is smaller than the image and fully visible.

	// caluclate the horizontal position of the dropon on the image
	if((align & MJ_ALIGN_LEFT) != 0) {
		position_x = 0;
	}
	else if((align & MJ_ALIGN_RIGHT) != 0) {
		position_x = m->width - d->width;
	}
	else {
		position_x = m->width / 2 - d->width / 2;
	}

	// add the horizontal offset to the position
	position_x += offset_x;

	// calculate the vertival position of the dropon on the image
	if((align & MJ_ALIGN_TOP) != 0) {
		position_y = 0;
	}
	else if((align & MJ_ALIGN_BOTTOM) != 0) {
		position_y = m->height - d->height;
	}
	else {
		position_y = m->height / 2 - d->height / 2;
	}

	// add the vertical offset to the position
	position_y += offset_y;

	// now that we have the position we can calculate how the
	// droppon needs to be cropped

	crop_x = 0;

	// if the dropon is off the left border of the image, we have to
	// crop this amount of pixel.
	if(position_x < 0) {
		crop_x = -position_x;
	}

	// the width of the crop area
	crop_w = d->width - crop_x;

	// is the dropon shifted more off the left border than its width?
	if(crop_x > d->width) {
		crop_w = 0;
	}
	// is the dropon shifted off the right border?
	else if(position_x > m->width) {
		crop_w = 0;
	}
	// is the dropon partially shifted off the right border?
	else if(position_x + crop_x + crop_w > m->width) {
		crop_w = m->width - crop_x - position_x;
	}

	crop_y = 0;

	// if the dropon is off the top border of the image, we have to
	// crop this amount of pixel.
	if(position_y < 0) {
		crop_y = -position_y;
	}

	// the height of the crop area
	crop_h = d->height - crop_y;

	// is the dropon shifted more off the top border than its height?
	if(crop_y > d->height) {
		crop_h = 0;
	}
	// is the dropon shifted off the bottom border?
	else if(position_y > m->height) {
		crop_h = 0;
	}
	// is the dropon partially shifted off the bottom border?
	else if(position_y + crop_y + crop_h > m->height) {
		crop_h = m->height - crop_y - position_y;
	}

	// we don't need to do anything if the crop width and height are zero
	if(crop_w == 0 || crop_h == 0) {
		return MJ_OK;
	}

	// the dropon has to align with the blocks of the image. here we calculate the
	// block offset which means how many pixels the dropon is off from the left and
	// top border of the block it has its top-left corner in. this area will be masked
	// out such that the image is not obstructed by the dropon.
	int blockoffset_x = position_x % m->sampling.h_factor;
	if(blockoffset_x < 0) {
		blockoffset_x = 0;
	}
	int blockoffset_y = position_y % m->sampling.v_factor;
	if(blockoffset_y < 0) {
		blockoffset_y = 0;
	}

	// with all these information together with the colorspace and sampling setting from the image
	// we can generate the apropriate dropon.
	mj_compileddropon_t cd;

	int rv = mj_compile_dropon(&cd, d, m->cinfo.jpeg_color_space, &m->sampling, blockoffset_x, blockoffset_y, crop_x, crop_y, crop_w, crop_h);
	if(rv != MJ_OK) {
		return rv;
	}

	// after the dropon is ready we calculate which block of the image the dropon starts
	int block_x = position_x / m->sampling.h_factor;
	int block_y = position_y / m->sampling.v_factor;

	if(block_x < 0) {
		block_x = 0;
	}

	if(block_y < 0) {
		block_y = 0;
	}

	// compoese the dropon and the image
	rv = mj_compose_with_mask(m, &cd, block_x, block_y);

	mj_free_compileddropon(&cd);

	return rv;
}

int mj_compose_without_mask(mj_jpeg_t *m, mj_compileddropon_t *cd, int block_x, int block_y) {
	if(m == NULL || cd == NULL) {
		return MJ_ERR_NULL_DATA;
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

		width_offset = block_x * component_m->h_samp_factor;
		height_offset = block_y * component_m->v_samp_factor;

		// copy the values from the dropon into the image
		for(l = 0; l < height_in_blocks; l++) {
			blocks_m = (*cinfo_m->mem->access_virt_barray)((j_common_ptr)&cinfo_m, m->coef[c], height_offset + l, 1, TRUE);

			for(k = 0; k < width_in_blocks; k++) {
				coefs_m = blocks_m[0][width_offset + k];
				imageblock = imagecomp->blocks[width_in_blocks * l + k];

				for(i = 0; i < DCTSIZE2; i += 8) {
					coefs_m[i + 0] = (int)imageblock[i + 0] / component_m->quant_table->quantval[i + 0];
					coefs_m[i + 1] = (int)imageblock[i + 1] / component_m->quant_table->quantval[i + 1];
					coefs_m[i + 2] = (int)imageblock[i + 2] / component_m->quant_table->quantval[i + 2];
					coefs_m[i + 3] = (int)imageblock[i + 3] / component_m->quant_table->quantval[i + 3];
					coefs_m[i + 4] = (int)imageblock[i + 4] / component_m->quant_table->quantval[i + 4];
					coefs_m[i + 5] = (int)imageblock[i + 5] / component_m->quant_table->quantval[i + 5];
					coefs_m[i + 6] = (int)imageblock[i + 6] / component_m->quant_table->quantval[i + 6];
					coefs_m[i + 7] = (int)imageblock[i + 7] / component_m->quant_table->quantval[i + 7];
				}
			}
		}

		break;
	}

	return MJ_OK;
}

int mj_compose_with_mask(mj_jpeg_t *m, mj_compileddropon_t *cd, int block_x, int block_y) {
	if(m == NULL || cd == NULL) {
		return MJ_ERR_NULL_DATA;
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

		width_offset = block_x * component_m->h_samp_factor;
		height_offset = block_y * component_m->v_samp_factor;

		// blend the values from the dropon with the image
		for(l = 0; l < height_in_blocks; l++) {
			blocks_m = (*cinfo_m->mem->access_virt_barray)((j_common_ptr)&cinfo_m, m->coef[c], height_offset + l, 1, TRUE);

			for(k = 0; k < width_in_blocks; k++) {
				coefs_m = blocks_m[0][width_offset + k];
				imageblock = imagecomp->blocks[width_in_blocks * l + k];
				alphablock = alphacomp->blocks[width_in_blocks * l + k];

				// de-quantize
				for(i = 0; i < DCTSIZE2; i += 8) {
					coefs_m[i + 0] *= component_m->quant_table->quantval[i + 0];
					coefs_m[i + 1] *= component_m->quant_table->quantval[i + 1];
					coefs_m[i + 2] *= component_m->quant_table->quantval[i + 2];
					coefs_m[i + 3] *= component_m->quant_table->quantval[i + 3];
					coefs_m[i + 4] *= component_m->quant_table->quantval[i + 4];
					coefs_m[i + 5] *= component_m->quant_table->quantval[i + 5];
					coefs_m[i + 6] *= component_m->quant_table->quantval[i + 6];
					coefs_m[i + 7] *= component_m->quant_table->quantval[i + 7];
				}

				// x = x0 - x1
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

				// quantize
				for(i = 0; i < DCTSIZE2; i += 8) {
					coefs_m[i + 0] /= component_m->quant_table->quantval[i + 0];
					coefs_m[i + 1] /= component_m->quant_table->quantval[i + 1];
					coefs_m[i + 2] /= component_m->quant_table->quantval[i + 2];
					coefs_m[i + 3] /= component_m->quant_table->quantval[i + 3];
					coefs_m[i + 4] /= component_m->quant_table->quantval[i + 4];
					coefs_m[i + 5] /= component_m->quant_table->quantval[i + 5];
					coefs_m[i + 6] /= component_m->quant_table->quantval[i + 6];
					coefs_m[i + 7] /= component_m->quant_table->quantval[i + 7];
				}
			}
		}
	}

	return MJ_OK;
}
