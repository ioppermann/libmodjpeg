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
#include <stdlib.h>

#include "libmodjpeg.h"
#include "image.h"
#include "dropon.h"

mj_dropon_t *mj_read_dropon_from_jpeg(const char *image_file, const char *alpha_file, short blend) {
	printf("entering %s\n", __FUNCTION__);

	char *image_buffer = NULL, *alpha_buffer = NULL, *buffer = NULL;
	int colorspace;
	int image_width = 0, alpha_width = 0;
	int image_height = 0, alpha_height = 0;
	size_t len = 0;

	if(mj_decode_jpeg_to_buffer(&image_buffer, &len, &image_width, &image_height, MJ_COLORSPACE_RGB, image_file) != 0) {
		printf("can't decode jpeg from %s\n", image_file);

		return NULL;
	}

	printf("image: %dx%d pixel, %ld bytes (%ld bytes/pixel)\n", image_width, image_height, len, len / (image_width * image_height));

	if(alpha_file != NULL) {
		if(mj_decode_jpeg_to_buffer(&alpha_buffer, &len, &alpha_width, &alpha_height, MJ_COLORSPACE_GRAYSCALE, alpha_file) != 0) {
			printf("can't decode jpeg from %s\n", alpha_file);

			free(image_buffer);
			return NULL;
		}

		printf("alpha: %dx%d pixel, %ld bytes (%ld bytes/pixel)\n", alpha_width, alpha_height, len, len / (alpha_width * alpha_height));

		if(image_width != alpha_width || image_height != alpha_height) {
			printf("error: dimensions of image and alpha need to be the same!\n");

			free(image_buffer);
			free(alpha_buffer);

			return NULL;
		}

		buffer = (char *)calloc(4 * image_width * image_height, sizeof(char));
		if(buffer == NULL) {
			printf("can't allocate memory for buffer\n");

			free(image_buffer);
			free(alpha_buffer);

			return NULL;
		}

		int v;
		char *t = buffer, *p = image_buffer, *q = alpha_buffer;

		for(v = 0; v < (image_width * image_height); v++) {
			*t++ = *p++;
			*t++ = *p++;
			*t++ = *p++;
			*t++ = *q++;
		}

		colorspace = MJ_COLORSPACE_RGBA; 
	}
	else {
		buffer = image_buffer;
		colorspace = MJ_COLORSPACE_RGB;
	}

	mj_dropon_t *d = mj_read_dropon_from_buffer(buffer, colorspace, image_width, image_height, blend);

	free(image_buffer);

	if(alpha_buffer != NULL) {
		free(alpha_buffer);
		free(buffer);
	}

	return d;
}

int mj_update_dropon(mj_dropon_t *d, J_COLOR_SPACE colorspace, mj_sampling_t *sampling, unsigned short offset) {
	printf("entering %s\n", __FUNCTION__);

	if(d == NULL) {
		return -1;
	}

	int i, j;

	if(d->image != NULL) {
		for(i = 0; i < d->image_ncomponents; i++) {
			mj_destroy_component(&d->image[i]);
		}

		free(d->image);
		d->image = NULL;
	}	

	if(d->alpha != NULL) {
		for(i = 0; i < d->alpha_ncomponents; i++) {
			mj_destroy_component(&d->alpha[i]);
		}

		free(d->alpha);
		d->alpha = NULL;
	}

	char *buffer = NULL;
	size_t len = 0;

	// hier offset berücksichtigen

	int h_offset = offset % sampling->h_factor;
	int v_offset = offset / sampling->v_factor;

	int raw_width = d->raw_width + sampling->h_factor;
	int raw_height = d->raw_height + sampling->v_factor;
	char *raw_data = (char *)calloc(3 * raw_width * raw_height, sizeof(char));
	if(raw_data == NULL) {
		return -1;
	}

	char *p, *q;

	for(i = 0; i < d->raw_height; i++) {
		p = &raw_data[(i + v_offset) * raw_width * 3 + (h_offset * 3)];
		q = &d->raw_image[i * d->raw_width * 3];

		for(j = 0; j < d->raw_width; j++) {
			*p++ = *q++;
			*p++ = *q++;
			*p++ = *q++;
		}
	}

	mj_encode_jpeg_to_buffer(&buffer, &len, (unsigned char *)raw_data, d->raw_colorspace, colorspace, sampling, raw_width, raw_height);
	printf("encoded len: %ld\n", len);

	mj_read_droponimage_from_buffer(d, buffer, len);
	free(buffer);

	// hier offset berücksichtigen

	for(i = 0; i < d->raw_height; i++) {
		p = &raw_data[(i + v_offset) * raw_width * 3 + (h_offset * 3)];
		q = &d->raw_alpha[i * d->raw_width * 3];

		for(j = 0; j < d->raw_width; j++) {
			*p++ = *q++;
			*p++ = *q++;
			*p++ = *q++;
		}
	}

	mj_encode_jpeg_to_buffer(&buffer, &len, (unsigned char *)raw_data, MJ_COLORSPACE_YCC, JCS_YCbCr, sampling, raw_width, raw_height);
	printf("encoded len: %ld\n", len);

	mj_read_droponalpha_from_buffer(d, buffer, len);
	free(buffer);

	free(raw_data);

	d->offset = offset;

	return 0;
}

int mj_read_droponimage_from_buffer(mj_dropon_t *d, const char *buffer, size_t len) {
	printf("entering %s\n", __FUNCTION__);

	if(d == NULL) {
		return 1;
	}

	mj_jpeg_t *m = mj_read_jpeg_from_buffer(buffer, len);
	if(m == NULL) {
		return 1;
	}

	int c, k, l, i;
	jpeg_component_info *component;
	mj_component_t *comp;
	mj_block_t *b;
	JBLOCKARRAY blocks;
	JCOEFPTR coefs;

	printf("dropon image: %dx%dpx, %d components: ", m->cinfo.output_width, m->cinfo.output_height, m->cinfo.num_components);

	d->image_ncomponents = m->cinfo.num_components;
	d->image_colorspace = m->cinfo.jpeg_color_space;
	d->image = (mj_component_t *)calloc(d->image_ncomponents, sizeof(mj_component_t));

	for(c = 0; c < m->cinfo.num_components; c++) {
		component = &m->cinfo.comp_info[c];
		comp = &d->image[c];

		printf("(%d,%d) ", component->h_samp_factor, component->v_samp_factor);

		comp->h_samp_factor = component->h_samp_factor;
		comp->v_samp_factor = component->v_samp_factor;

		comp->width_in_blocks = m->h_blocks * component->h_samp_factor;
		comp->height_in_blocks = m->v_blocks * component->v_samp_factor;

		comp->nblocks = comp->width_in_blocks * comp->height_in_blocks;
		comp->blocks = (mj_block_t **)calloc(comp->nblocks, sizeof(mj_block_t *));

		printf("%dx%d blocks ", comp->width_in_blocks, comp->height_in_blocks);

		for(l = 0; l < comp->height_in_blocks; l++) {
			blocks = (*m->cinfo.mem->access_virt_barray)((j_common_ptr)&m->cinfo, m->coef[c], l, 1, TRUE);

			for(k = 0; k < comp->width_in_blocks; k++) {
				comp->blocks[comp->height_in_blocks * l + k] = (mj_block_t *)calloc(64, sizeof(mj_block_t));
				b = comp->blocks[comp->height_in_blocks * l + k];

				coefs = blocks[0][k];

/*
				printf("component %d (%d,%d)\n", c, l, k);

				int p, q;
				for(p = 0; p < DCTSIZE; p++) {
					for(q = 0; q < DCTSIZE; q++) {
						printf("%4d ", coefs[DCTSIZE * p + q]);
					}
					printf("\n");
				}
*/
				for(i = 0; i < DCTSIZE2; i += 8) {
					b[i + 0] = (float)coefs[i + 0];
					b[i + 1] = (float)coefs[i + 1];
					b[i + 2] = (float)coefs[i + 2];
					b[i + 3] = (float)coefs[i + 3];
					b[i + 4] = (float)coefs[i + 4];
					b[i + 5] = (float)coefs[i + 5];
					b[i + 6] = (float)coefs[i + 6];
					b[i + 7] = (float)coefs[i + 7];
				}
			}
		}
	}

	printf("\n");

	mj_write_jpeg_to_file(m, "./images/dropon_image.jpg");

	mj_destroy_jpeg(m);

	return 0;
}

int mj_read_droponalpha_from_buffer(mj_dropon_t *d, const char *buffer, size_t len) {
	printf("entering %s\n", __FUNCTION__);

	if(d == NULL) {
		return 1;
	}

	mj_jpeg_t *m = mj_read_jpeg_from_buffer(buffer, len);
	if(m == NULL) {
		return 1;
	}

	int c, k, l, i;
	jpeg_component_info *component;
	mj_component_t *comp;
	mj_block_t *b;
	JBLOCKARRAY blocks;
	JCOEFPTR coefs;

	printf("dropon alpha: %dx%dpx, %d components: ", m->cinfo.output_width, m->cinfo.output_height, m->cinfo.num_components);

	d->alpha_ncomponents = m->cinfo.num_components;
	d->alpha = (mj_component_t *)calloc(d->alpha_ncomponents, sizeof(mj_component_t));

	for(c = 0; c < m->cinfo.num_components; c++) {
		component = &m->cinfo.comp_info[c];
		comp = &d->alpha[c];

		printf("(%d,%d) ", component->h_samp_factor, component->v_samp_factor);

		comp->h_samp_factor = component->h_samp_factor;
		comp->v_samp_factor = component->v_samp_factor;

		comp->width_in_blocks = m->h_blocks * component->h_samp_factor;
		comp->height_in_blocks = m->v_blocks * component->v_samp_factor;

		comp->nblocks = comp->width_in_blocks * comp->height_in_blocks;
		comp->blocks = (mj_block_t **)calloc(comp->nblocks, sizeof(mj_block_t *));

		printf("%dx%d blocks ", comp->width_in_blocks, comp->height_in_blocks);

		for(l = 0; l < comp->height_in_blocks; l++) {
			blocks = (*m->cinfo.mem->access_virt_barray)((j_common_ptr)&m->cinfo, m->coef[c], l, 1, TRUE);

			for(k = 0; k < comp->width_in_blocks; k++) {
				comp->blocks[comp->height_in_blocks * l + k] = (mj_block_t *)calloc(64, sizeof(mj_block_t));
				b = comp->blocks[comp->height_in_blocks * l + k];

				coefs = blocks[0][k];

				coefs[0] += 1024;
/*
				printf("component %d (%d,%d)\n", c, l, k);

				int p, q;
				for(p = 0; p < DCTSIZE; p++) {
					for(q = 0; q < DCTSIZE; q++) {
						printf("%4d ", coefs[DCTSIZE * p + q]);
					}
					printf("\n");
				}
*/
				/*
				// w'(j, i) = w(j, i) * 1/255 * c(i) * c(j) * 1/4
				// Der Faktor 1/4 kommt von den V(i) und V(j)
				// => 1/255 * 1/4 = 1/1020
				*/
				b[0] = (float)coefs[0] * (0.3535534 * 0.3535534 / 1020.0);
				b[1] = (float)coefs[1] * (0.3535534 * 0.5 / 1020.0);
				b[2] = (float)coefs[2] * (0.3535534 * 0.5 / 1020.0);
				b[3] = (float)coefs[3] * (0.3535534 * 0.5 / 1020.0);
				b[4] = (float)coefs[4] * (0.3535534 * 0.5 / 1020.0);
				b[5] = (float)coefs[5] * (0.3535534 * 0.5 / 1020.0);
				b[6] = (float)coefs[6] * (0.3535534 * 0.5 / 1020.0);
				b[7] = (float)coefs[7] * (0.3535534 * 0.5 / 1020.0);

				for(i = 8; i < DCTSIZE2; i += 8) {
					b[i + 0] = (float)coefs[i + 0] * (0.5 * 0.3535534 / 1020.0);
					b[i + 1] = (float)coefs[i + 1] * (0.5 * 0.5 / 1020.0);
					b[i + 2] = (float)coefs[i + 2] * (0.5 * 0.5 / 1020.0);
					b[i + 3] = (float)coefs[i + 3] * (0.5 * 0.5 / 1020.0);
					b[i + 4] = (float)coefs[i + 4] * (0.5 * 0.5 / 1020.0);
					b[i + 5] = (float)coefs[i + 5] * (0.5 * 0.5 / 1020.0);
					b[i + 6] = (float)coefs[i + 6] * (0.5 * 0.5 / 1020.0);
					b[i + 7] = (float)coefs[i + 7] * (0.5 * 0.5 / 1020.0);
				}
			}
		}
	}

	printf("\n");

	mj_write_jpeg_to_file(m, "./images/dropon_alpha.jpg");

	mj_destroy_jpeg(m);

	return 0;
}

mj_dropon_t *mj_read_dropon_from_buffer(const char *raw_data, unsigned int colorspace, size_t width, size_t height, short blend) {
	printf("entering %s\n", __FUNCTION__);

	mj_dropon_t *d;

	if(raw_data == NULL) {
		return NULL;
	}

	if(blend < MJ_BLEND_NONE) {
		blend = MJ_BLEND_NONE;
	}
	else if(blend > MJ_BLEND_FULL) {
		blend = MJ_BLEND_FULL;
	}

	switch(colorspace) {
		case MJ_COLORSPACE_RGB:
		case MJ_COLORSPACE_RGBA:
		case MJ_COLORSPACE_YCC:
		case MJ_COLORSPACE_YCCA:
		case MJ_COLORSPACE_GRAYSCALE:
		case MJ_COLORSPACE_GRAYSCALEA:
			break;
		default:
			printf("unsupported colorspace");
			return NULL;
	}

	d = (mj_dropon_t *)calloc(1, sizeof(mj_dropon_t));
	if(d == NULL) {
		printf("can't allocate dropon");
		return NULL;
	}

	d->raw_width = width;
	d->raw_height = height;
	d->blend = blend;

	// image
	size_t nsamples = 3 * width * height;

	d->raw_image = (char *)calloc(nsamples, sizeof(char));
	if(d->raw_image == NULL) {
		free(d);
		printf("can't allocate buffer");
		return NULL;
	}

	nsamples = 3 * width * height;

	d->raw_alpha = (char *)calloc(nsamples, sizeof(char));
	if(d->raw_alpha == NULL) {
		free(d->raw_image);
		free(d);
		printf("can't allocate buffer");
		return NULL;
	}

	const char *p = raw_data;
	char *pimage = d->raw_image;
	char *palpha = d->raw_alpha;

	size_t v = 0;

	if(colorspace == MJ_COLORSPACE_RGBA || colorspace == MJ_COLORSPACE_YCCA) {
		for(v = 0; v < (width * height); v++) {
			*pimage++ = *p++;
			*pimage++ = *p++;
			*pimage++ = *p++;

			*palpha++ = *p;
			*palpha++ = *p;
			*palpha++ = *p++;
		}

		if(colorspace == MJ_COLORSPACE_RGBA) {
			d->raw_colorspace = MJ_COLORSPACE_RGB;
		}
		else {
			d->raw_colorspace = MJ_COLORSPACE_YCC;
		}

		d->blend = MJ_BLEND_NONUNIFORM;
	}
	else if(colorspace == MJ_COLORSPACE_RGB || colorspace == MJ_COLORSPACE_YCC) {
		for(v = 0; v < (width * height); v++) {
			*pimage++ = *p++;
			*pimage++ = *p++;
			*pimage++ = *p++;

			*palpha++ = (char)d->blend;
			*palpha++ = (char)d->blend;
			*palpha++ = (char)d->blend;
		}

		d->raw_colorspace = colorspace;
	}
	else if(colorspace == MJ_COLORSPACE_GRAYSCALEA) {
		for(v = 0; v < (width * height); v++) {
			*pimage++ = *p;
			*pimage++ = *p;
			*pimage++ = *p++;

			*palpha++ = *p;
			*palpha++ = *p;
			*palpha++ = *p++;
		}

		d->raw_colorspace = MJ_COLORSPACE_GRAYSCALE;

		d->blend = MJ_BLEND_NONUNIFORM;
	}
	else {
		for(v = 0; v < (width * height); v++) {
			*pimage++ = *p;
			*pimage++ = *p;
			*pimage++ = *p++;

			*palpha++ = (char)d->blend;
			*palpha++ = (char)d->blend;
			*palpha++ = (char)d->blend;
		}

		d->raw_colorspace = MJ_COLORSPACE_GRAYSCALE;
	}

	return d;
}

void mj_destroy_dropon(mj_dropon_t *d) {
	printf("entering %s\n", __FUNCTION__);

	if(d == NULL) {
		return;
	}

	if(d->raw_image != NULL) {
		free(d->raw_image);
		d->raw_image = NULL;
	}

	if(d->raw_alpha != NULL) {
		free(d->raw_alpha);
		d->raw_alpha = NULL;
	}

	int i;

	if(d->image != NULL) {
		for(i = 0; i < d->image_ncomponents; i++) {
			mj_destroy_component(&d->image[i]);
		}
		free(d->image);
		d->image = NULL;
	}

	if(d->alpha != NULL) {
		for(i = 0; i < d->alpha_ncomponents; i++) {
			mj_destroy_component(&d->alpha[i]);
		}
		free(d->alpha);
		d->alpha = NULL;
	}

	return;
}

void mj_destroy_component(mj_component_t *c) {
	printf("entering %s\n", __FUNCTION__);

	if(c == NULL) {
		return;
	}

	int i;

	for(i = 0; i < c->nblocks; i++) {
		free(c->blocks[i]);
	}

	free(c->blocks);

	return;
}
