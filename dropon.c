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
#include <stdlib.h>

#include "libmodjpeg.h"
#include "image.h"
#include "dropon.h"

mj_dropon_t *mj_read_dropon_from_jpeg(const char *image_file, const char *alpha_file, short blend) {
	fprintf(stderr, "entering %s\n", __FUNCTION__);

	char *image_buffer = NULL, *alpha_buffer = NULL, *buffer = NULL;
	int colorspace;
	int image_width = 0, alpha_width = 0;
	int image_height = 0, alpha_height = 0;
	size_t len = 0;

	if(mj_decode_jpeg_to_buffer(&image_buffer, &len, &image_width, &image_height, MJ_COLORSPACE_RGB, image_file) != MJ_OK) {
		fprintf(stderr, "can't decode jpeg from %s\n", image_file);

		return NULL;
	}

	fprintf(stderr, "image: %dx%d pixel, %ld bytes (%ld bytes/pixel)\n", image_width, image_height, len, len / (image_width * image_height));

	if(alpha_file != NULL) {
		if(mj_decode_jpeg_to_buffer(&alpha_buffer, &len, &alpha_width, &alpha_height, MJ_COLORSPACE_GRAYSCALE, alpha_file) != MJ_OK) {
			fprintf(stderr, "can't decode jpeg from %s\n", alpha_file);

			free(image_buffer);
			return NULL;
		}

		fprintf(stderr, "alpha: %dx%d pixel, %ld bytes (%ld bytes/pixel)\n", alpha_width, alpha_height, len, len / (alpha_width * alpha_height));

		if(image_width != alpha_width || image_height != alpha_height) {
			fprintf(stderr, "error: dimensions of image and alpha need to be the same!\n");

			free(image_buffer);
			free(alpha_buffer);

			return NULL;
		}

		buffer = (char *)calloc(4 * image_width * image_height, sizeof(char));
		if(buffer == NULL) {
			fprintf(stderr, "can't allocate memory for buffer\n");

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

mj_dropon_t *mj_read_dropon_from_buffer(const char *raw_data, unsigned int colorspace, size_t width, size_t height, short blend) {
	fprintf(stderr, "entering %s\n", __FUNCTION__);

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
			fprintf(stderr, "unsupported colorspace");
			return NULL;
	}

	d = (mj_dropon_t *)calloc(1, sizeof(mj_dropon_t));
	if(d == NULL) {
		fprintf(stderr, "can't allocate dropon");
		return NULL;
	}

	d->width = width;
	d->height = height;
	d->blend = blend;

	// image and alpha are store with 3 components. this makes it
	// easier to handle later for compiling the dropon.
	size_t nsamples = 3 * width * height;

	d->image = (char *)calloc(nsamples, sizeof(char));
	if(d->image == NULL) {
		free(d);
		fprintf(stderr, "can't allocate buffer");
		return NULL;
	}

	// the alpha channel is also stored with 3 component
	d->alpha = (char *)calloc(nsamples, sizeof(char));
	if(d->alpha == NULL) {
		free(d->image);
		free(d);
		fprintf(stderr, "can't allocate buffer");
		return NULL;
	}

	const char *p = raw_data;
	char *pimage = d->image;
	char *palpha = d->alpha;

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
			d->colorspace = MJ_COLORSPACE_RGB;
		}
		else {
			d->colorspace = MJ_COLORSPACE_YCC;
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

		d->colorspace = colorspace;
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

		d->colorspace = MJ_COLORSPACE_GRAYSCALE;

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

		d->colorspace = MJ_COLORSPACE_GRAYSCALE;
	}

	return d;
}

int mj_compile_dropon(mj_compileddropon_t *cd, mj_dropon_t *d, J_COLOR_SPACE colorspace, mj_sampling_t *sampling, int blockoffset_x, int blockoffset_y, int crop_x, int crop_y, int crop_w, int crop_h) {
	fprintf(stderr, "entering %s\n", __FUNCTION__);

	if(cd == NULL || d == NULL) {
		return MJ_ERR;
	}

	// crop and or extend the dropon. the dropon needs to cover whole blocks.

	// after that encode it to a jpeg with the same colorspace and sampling as the image.
	// this gives us the the dropon in frequency space.

	// same for the mask. the mask is required if we extend the dropon such that
	// the extended area doesn't cover the image.

	int i, j;

	char *buffer = NULL;
	size_t len = 0;

	// crop/extend the dropon

	fprintf(stderr, "crop (%d, %d, %d, %d)\n", crop_x, crop_y, crop_w, crop_h);

	int width = crop_w + blockoffset_x;
	int padding = width % sampling->h_factor;
	if(padding != 0) {
		width += sampling->h_factor - padding;
	}

	int height = crop_h + blockoffset_y;
	padding = height % sampling->v_factor;
	if(padding != 0) {
		height += sampling->v_factor - padding;
	}

	fprintf(stderr, "(width, height) = (%d, %d)\n", width, height);

	char *data = (char *)calloc(3 * width * height, sizeof(char));
	if(data == NULL) {
		return MJ_ERR;
	}

	int rv;
	char *p, *q;

	for(i = crop_y; i < (crop_y + crop_h); i++) {
		p = &data[(i - crop_y + blockoffset_y) * width * 3 + (blockoffset_x * 3)];
		q = &d->image[i * d->width * 3 + (crop_x * 3)];

		for(j = crop_x; j < (crop_x + crop_w); j++) {
			*p++ = *q++;
			*p++ = *q++;
			*p++ = *q++;
		}
	}

	// encode the dropon to JPEG
	rv = mj_encode_jpeg_to_buffer(&buffer, &len, (unsigned char *)data, d->colorspace, colorspace, sampling, width, height);
	if(rv != MJ_OK) {
		free(data);
		return rv;
	}

	fprintf(stderr, "encoded len: %ld\n", len);

	// read the coefficients from the encoded dropon
	rv = mj_read_droponimage_from_buffer(cd, buffer, len);
	free(buffer);

	if(rv != MJ_OK) {
		free(data);
		return rv;
	}

	for(i = crop_y; i < (crop_y + crop_h); i++) {
		p = &data[(i - crop_y + blockoffset_y) * width * 3 + (blockoffset_x * 3)];
		q = &d->alpha[i * d->width * 3 + (crop_x * 3)];

		for(j = crop_x; j < (crop_x + crop_w); j++) {
			*p++ = *q++;
			*p++ = *q++;
			*p++ = *q++;
		}
	}

	// encode the mask to JPEG. the mask is always in YCC colorspace (i.e. only the Y component is used)
	rv = mj_encode_jpeg_to_buffer(&buffer, &len, (unsigned char *)data, MJ_COLORSPACE_YCC, JCS_YCbCr, sampling, width, height);
	if(rv != MJ_OK) {
		free(data);
		return rv;
	}

	fprintf(stderr, "encoded len: %ld\n", len);

	// read the coefficients from the encoded dropon mask
	rv = mj_read_droponalpha_from_buffer(cd, buffer, len);

	free(buffer);
	free(data);

	return rv;
}

int mj_read_droponimage_from_buffer(mj_compileddropon_t *cd, const char *buffer, size_t len) {
	fprintf(stderr, "entering %s\n", __FUNCTION__);

	if(cd == NULL) {
		return MJ_ERR;
	}

	mj_jpeg_t *m = mj_read_jpeg_from_buffer(buffer, len);
	if(m == NULL) {
		return MJ_ERR;
	}

	int c, k, l, i;
	jpeg_component_info *component;
	mj_component_t *comp;
	mj_block_t *b;
	JBLOCKARRAY blocks;
	JCOEFPTR coefs;

	fprintf(stderr, "dropon image: %dx%dpx, %d components: ", m->cinfo.output_width, m->cinfo.output_height, m->cinfo.num_components);

	cd->image_ncomponents = m->cinfo.num_components;
	cd->image_colorspace = m->cinfo.jpeg_color_space;
	cd->image = (mj_component_t *)calloc(cd->image_ncomponents, sizeof(mj_component_t));

	for(c = 0; c < m->cinfo.num_components; c++) {
		component = &m->cinfo.comp_info[c];
		comp = &cd->image[c];

		fprintf(stderr, "(%d,%d) ", component->h_samp_factor, component->v_samp_factor);

		comp->h_samp_factor = component->h_samp_factor;
		comp->v_samp_factor = component->v_samp_factor;

		comp->width_in_blocks = component->width_in_blocks;
		comp->height_in_blocks = component->height_in_blocks;

		comp->nblocks = comp->width_in_blocks * comp->height_in_blocks;
		comp->blocks = (mj_block_t **)calloc(comp->nblocks, sizeof(mj_block_t *));

		fprintf(stderr, "%dx%d blocks ", comp->width_in_blocks, comp->height_in_blocks);

		for(l = 0; l < comp->height_in_blocks; l++) {
			blocks = (*m->cinfo.mem->access_virt_barray)((j_common_ptr)&m->cinfo, m->coef[c], l, 1, TRUE);

			for(k = 0; k < comp->width_in_blocks; k++) {
				b = (mj_block_t *)calloc(64, sizeof(mj_block_t));
				coefs = blocks[0][k];
/*
				fprintf(stderr, "\ncomponent %d (%d,%d) %p\n", c, l, k, b);

				int p, q;
				for(p = 0; p < DCTSIZE; p++) {
					for(q = 0; q < DCTSIZE; q++) {
						fprintf(stderr, "%5d ", coefs[DCTSIZE * p + q]);
					}
					fprintf(stderr, "\n");
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

				comp->blocks[comp->width_in_blocks * l + k] = b;
			}
		}
	}

	fprintf(stderr, "\n");

	//mj_write_jpeg_to_file(m, "./images/dropon_image.jpg", MJ_OPTION_NONE);

	mj_destroy_jpeg(m);

	return MJ_OK;
}

int mj_read_droponalpha_from_buffer(mj_compileddropon_t *cd, const char *buffer, size_t len) {
	fprintf(stderr, "entering %s\n", __FUNCTION__);

	if(cd == NULL) {
		return MJ_ERR;
	}

	mj_jpeg_t *m = mj_read_jpeg_from_buffer(buffer, len);
	if(m == NULL) {
		return MJ_ERR;
	}

	int c, k, l, i;
	jpeg_component_info *component;
	mj_component_t *comp;
	mj_block_t *b;
	JBLOCKARRAY blocks;
	JCOEFPTR coefs;

	fprintf(stderr, "dropon alpha: %dx%dpx, %d components: ", m->cinfo.output_width, m->cinfo.output_height, m->cinfo.num_components);

	cd->alpha_ncomponents = m->cinfo.num_components;
	cd->alpha = (mj_component_t *)calloc(cd->alpha_ncomponents, sizeof(mj_component_t));

	for(c = 0; c < m->cinfo.num_components; c++) {
		component = &m->cinfo.comp_info[c];
		comp = &cd->alpha[c];

		fprintf(stderr, "(%d,%d) ", component->h_samp_factor, component->v_samp_factor);

		comp->h_samp_factor = component->h_samp_factor;
		comp->v_samp_factor = component->v_samp_factor;

		comp->width_in_blocks = component->width_in_blocks;
		comp->height_in_blocks = component->height_in_blocks;

		comp->nblocks = comp->width_in_blocks * comp->height_in_blocks;
		comp->blocks = (mj_block_t **)calloc(comp->nblocks, sizeof(mj_block_t *));

		fprintf(stderr, "%dx%d blocks ", comp->width_in_blocks, comp->height_in_blocks);

		for(l = 0; l < comp->height_in_blocks; l++) {
			blocks = (*m->cinfo.mem->access_virt_barray)((j_common_ptr)&m->cinfo, m->coef[c], l, 1, TRUE);

			for(k = 0; k < comp->width_in_blocks; k++) {
				b = (mj_block_t *)calloc(64, sizeof(mj_block_t));
				coefs = blocks[0][k];

				coefs[0] += 1024;
/*
				fprintf(stderr, "component %d (%d,%d)\n", c, l, k);

				int p, q;
				for(p = 0; p < DCTSIZE; p++) {
					for(q = 0; q < DCTSIZE; q++) {
						fprintf(stderr, "%4d ", coefs[DCTSIZE * p + q]);
					}
					fprintf(stderr, "\n");
				}
*/
				/*
				// w'(j, i) = w(j, i) * 1/255 * c(i) * c(j) * 1/4
				// the factor 1/4 comes from V(i) and V(j)
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

				comp->blocks[comp->width_in_blocks * l + k] = b;
			}
		}
	}

	fprintf(stderr, "\n");

	//mj_write_jpeg_to_file(m, "./images/dropon_alpha.jpg", MJ_OPTION_NONE);

	mj_destroy_jpeg(m);

	return MJ_OK;
}

void mj_destroy_dropon(mj_dropon_t *d) {
	fprintf(stderr, "entering %s\n", __FUNCTION__);

	if(d == NULL) {
		return;
	}

	if(d->image != NULL) {
		free(d->image);
		d->image = NULL;
	}

	if(d->alpha != NULL) {
		free(d->alpha);
		d->alpha = NULL;
	}

	free(d);

	return;
}

void mj_free_compileddropon(mj_compileddropon_t *cd) {
	fprintf(stderr, "entering %s\n", __FUNCTION__);

	if(cd == NULL) {
		return;
	}

	int i;

	if(cd->image != NULL) {
		for(i = 0; i < cd->image_ncomponents; i++) {
			mj_free_component(&cd->image[i]);
		}
		free(cd->image);
		cd->image = NULL;
	}

	if(cd->alpha != NULL) {
		for(i = 0; i < cd->alpha_ncomponents; i++) {
			mj_free_component(&cd->alpha[i]);
		}
		free(cd->alpha);
		cd->alpha = NULL;
	}

	return;
}

void mj_free_component(mj_component_t *c) {
	fprintf(stderr, "entering %s\n", __FUNCTION__);

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
