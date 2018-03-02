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
#include <sys/stat.h>

#include "libmodjpeg.h"
#include "image.h"
#include "jpeg.h"

int mj_write_jpeg_to_buffer(mj_jpeg_t *m, char **buffer, size_t *len, int options) {
	fprintf(stderr, "entering %s\n", __FUNCTION__);

	struct jpeg_compress_struct cinfo;
	jvirt_barray_ptr *dst_coef_arrays;
	struct mj_jpeg_error_mgr jerr;
	struct mj_jpeg_dest_mgr dest;
	char jpegerrorbuffer[JMSG_LENGTH_MAX];

	cinfo.err = jpeg_std_error(&jerr.pub);
	jerr.pub.error_exit = mj_jpeg_error_exit;
	if(setjmp(jerr.setjmp_buffer)) {
		(*cinfo.err->format_message)((j_common_ptr)&cinfo, jpegerrorbuffer);
		jpeg_destroy_compress(&cinfo);
		if(dest.buf != NULL) {
			free(dest.buf);
		}

		return MJ_ERR;
	}

	int c, k, l, i;
	int width_in_blocks, height_in_blocks;
	jpeg_component_info *component;
	JBLOCKARRAY blocks;
	JCOEFPTR coefs;

	for(c = 0; c < m->cinfo.num_components; c++) {
		component = &m->cinfo.comp_info[c];

		//fprintf(stderr, "(%d,%d)", component->h_samp_factor, component->v_samp_factor);

		width_in_blocks = component->width_in_blocks;
		height_in_blocks = component->height_in_blocks;

		// quantize coefficients
		for(l = 0; l < height_in_blocks; l++) {
			blocks = (*m->cinfo.mem->access_virt_barray)((j_common_ptr)&m->cinfo, m->coef[c], l, 1, TRUE);

			for(k = 0; k < width_in_blocks; k++) {
				coefs = blocks[0][k];

				for(i = 0; i < DCTSIZE2; i += 8) {
					coefs[i + 0] /= component->quant_table->quantval[i + 0];
					coefs[i + 1] /= component->quant_table->quantval[i + 1];
					coefs[i + 2] /= component->quant_table->quantval[i + 2];
					coefs[i + 3] /= component->quant_table->quantval[i + 3];
					coefs[i + 4] /= component->quant_table->quantval[i + 4];
					coefs[i + 5] /= component->quant_table->quantval[i + 5];
					coefs[i + 6] /= component->quant_table->quantval[i + 6];
					coefs[i + 7] /= component->quant_table->quantval[i + 7];
				}
			}
		}
	}

	jpeg_create_compress(&cinfo);

	cinfo.dest = &dest.pub;
	dest.buf = NULL;
	dest.size = 0;
	dest.pub.init_destination = mj_jpeg_init_destination;
	dest.pub.empty_output_buffer = mj_jpeg_empty_output_buffer;
	dest.pub.term_destination = mj_jpeg_term_destination;

	jpeg_copy_critical_parameters(&m->cinfo, &cinfo);

	if((options & MJ_OPTION_OPTIMIZE) != 0) {
		cinfo.optimize_coding = TRUE;
	}

	if((options & MJ_OPTION_ARITHMETRIC) != 0) {
		cinfo.arith_code = TRUE;
	}

	if((options & MJ_OPTION_PROGRESSIVE) != 0) {
		jpeg_simple_progression(&cinfo);
	}

	dst_coef_arrays = m->coef;

	// save the new coefficients
	jpeg_write_coefficients(&cinfo, dst_coef_arrays);

	jpeg_finish_compress(&cinfo);
	jpeg_destroy_compress(&cinfo);

	*buffer = (char *)dest.buf;
	*len = dest.size;

	return MJ_OK;
}

int mj_write_jpeg_to_file(mj_jpeg_t *m, char *filename, int options) {
	fprintf(stderr, "entering %s\n", __FUNCTION__);

	FILE *fp;
	char *rebuffer = NULL;
	size_t relen = 0;

	if(m == NULL) {
		fprintf(stderr, "jpegimage not given\n");
		return MJ_ERR;
	}

	fp = fopen(filename, "wb");
	if(fp == NULL) {
		fprintf(stderr, "can't open output file\n");
		return MJ_ERR;
	}

	mj_write_jpeg_to_buffer(m, &rebuffer, &relen, options);

	fprintf(stderr, "restored image of len %ld\n", relen);

	fwrite(rebuffer, 1, relen, fp);

	fclose(fp);

	free(rebuffer);

	return 0;
}

mj_jpeg_t *mj_read_jpeg_from_buffer(const char *buffer, size_t len) {
	fprintf(stderr, "entering %s\n", __FUNCTION__);

	mj_jpeg_t *m;
	struct mj_jpeg_error_mgr jerr;
	struct mj_jpeg_src_mgr src;
	//jpeg_component_info *component;

	if(buffer == NULL || len == 0) {
		fprintf(stderr, "empty buffer\n");
		return NULL;
	}

	m = (mj_jpeg_t *)calloc(1, sizeof(mj_jpeg_t));
	if(m == NULL) {
		return NULL;
	}

	m->cinfo.err = jpeg_std_error(&jerr.pub);
	jerr.pub.error_exit = mj_jpeg_error_exit;
	if(setjmp(jerr.setjmp_buffer)) {
		jpeg_destroy_decompress(&m->cinfo);
		free(m);
		return NULL;
	}

	jpeg_create_decompress(&m->cinfo);

	m->cinfo.src = &src.pub;
	src.pub.init_source = mj_jpeg_init_source;
	src.pub.fill_input_buffer = mj_jpeg_fill_input_buffer;
	src.pub.skip_input_data = mj_jpeg_skip_input_data;
	src.pub.resync_to_restart = jpeg_resync_to_restart;
	src.pub.term_source = mj_jpeg_term_source;

	src.buf = (JOCTET *)buffer;
	src.size = len;

	jpeg_read_header(&m->cinfo, TRUE);

	switch(m->cinfo.jpeg_color_space) {
		case JCS_GRAYSCALE:
			fprintf(stderr, "colorspace: GRAYSCALE (%d)\n", JCS_GRAYSCALE);
			break;
		case JCS_RGB:
			fprintf(stderr, "colorspace: RGB (%d)\n", JCS_RGB);
			break;
		case JCS_YCbCr:
			fprintf(stderr, "colorspace: YCbCr (%d)\n", JCS_YCbCr);
			break;
		default:
			jpeg_destroy_decompress(&m->cinfo);
			free(m);
			return NULL;
	}

	m->coef = jpeg_read_coefficients(&m->cinfo);

	m->width = m->cinfo.output_width;
	m->height = m->cinfo.output_height;

	fprintf(stderr, "%dx%dpx, %d components: ", m->width, m->height, m->cinfo.num_components);

	m->sampling.max_h_samp_factor = m->cinfo.max_h_samp_factor;
	m->sampling.max_v_samp_factor = m->cinfo.max_v_samp_factor;

	m->sampling.h_factor = (m->sampling.max_h_samp_factor * DCTSIZE);
	m->sampling.v_factor = (m->sampling.max_v_samp_factor * DCTSIZE);

	int c, k, l, i;
	int width_in_blocks, height_in_blocks;
	jpeg_component_info *component;
	JBLOCKARRAY blocks;
	JCOEFPTR coefs;

	for(c = 0; c < m->cinfo.num_components; c++) {
		component = &m->cinfo.comp_info[c];

		m->sampling.samp_factor[c].h_samp_factor = component->h_samp_factor;
		m->sampling.samp_factor[c].v_samp_factor = component->v_samp_factor;

		width_in_blocks = component->width_in_blocks;
		height_in_blocks = component->height_in_blocks;

		fprintf(stderr, "(%d,%d) %dx%d blocks ", component->h_samp_factor, component->v_samp_factor, width_in_blocks, height_in_blocks);

		// de-quantize coefficients
		for(l = 0; l < height_in_blocks; l++) {
			blocks = (*m->cinfo.mem->access_virt_barray)((j_common_ptr)&m->cinfo, m->coef[c], l, 1, TRUE);

			for(k = 0; k < width_in_blocks; k++) {
				coefs = blocks[0][k];

				for(i = 0; i < DCTSIZE2; i += 8) {
					coefs[i + 0] *= component->quant_table->quantval[i + 0];
					coefs[i + 1] *= component->quant_table->quantval[i + 1];
					coefs[i + 2] *= component->quant_table->quantval[i + 2];
					coefs[i + 3] *= component->quant_table->quantval[i + 3];
					coefs[i + 4] *= component->quant_table->quantval[i + 4];
					coefs[i + 5] *= component->quant_table->quantval[i + 5];
					coefs[i + 6] *= component->quant_table->quantval[i + 6];
					coefs[i + 7] *= component->quant_table->quantval[i + 7];
				}
			}
		}
	}

	fprintf(stderr, "\n");

	return m;
}

int mj_decode_jpeg_to_buffer(char **buffer, size_t *len, int *width, int *height, int want_colorspace, const char *filename) {
	fprintf(stderr, "entering %s\n", __FUNCTION__);

	FILE *fp;
	struct jpeg_decompress_struct cinfo;
	struct mj_jpeg_error_mgr jerr;
	char jpegerrorbuffer[JMSG_LENGTH_MAX];

	cinfo.err = jpeg_std_error(&jerr.pub);
	jerr.pub.error_exit = mj_jpeg_error_exit;
	if(setjmp(jerr.setjmp_buffer)) {
		(*cinfo.err->format_message)((j_common_ptr)&cinfo, jpegerrorbuffer);
		jpeg_destroy_decompress(&cinfo);
		fclose(fp);
		return MJ_ERR;
	}

	jpeg_create_decompress(&cinfo);

	fp = fopen(filename, "rb");
	if(fp == NULL) {
		jpeg_destroy_decompress(&cinfo);
		return MJ_ERR;
	}

	jpeg_stdio_src(&cinfo, fp);

	jpeg_read_header(&cinfo, TRUE);

	switch(want_colorspace) {
		case MJ_COLORSPACE_RGB:
			cinfo.out_color_space = JCS_RGB;
			break;
		case MJ_COLORSPACE_YCC:
			cinfo.out_color_space = JCS_YCbCr;
			break;
		case MJ_COLORSPACE_GRAYSCALE:
			cinfo.out_color_space = JCS_GRAYSCALE;
			break;
		default:
			fprintf(stderr, "error: unsupported colorspace (%d)\n", want_colorspace);
			jpeg_destroy_decompress(&cinfo);
			return MJ_ERR;
	}

	jpeg_start_decompress(&cinfo);

	*width = cinfo.output_width;
	*height = cinfo.output_height;

	fprintf(stderr, "decode jpeg: %dx%dpx with %d components\n", *width, *height, cinfo.output_components);

	int row_stride = cinfo.output_width * cinfo.output_components;

	*len = row_stride * cinfo.output_height;

	unsigned char *buf = (unsigned char *)calloc(row_stride * cinfo.output_height, sizeof(unsigned char));

	JSAMPROW row_pointer[1];

	while(cinfo.output_scanline < cinfo.output_height) {
		row_pointer[0] = &buf[cinfo.output_scanline * row_stride];
		jpeg_read_scanlines(&cinfo, row_pointer, 1);
	}

	jpeg_finish_decompress(&cinfo);
	jpeg_destroy_decompress(&cinfo);

	*buffer = (char *)buf;

	return MJ_OK;
}

mj_jpeg_t *mj_read_jpeg_from_file(const char *filename) {
	fprintf(stderr, "entering %s\n", __FUNCTION__);

	FILE *fp;
	struct stat s;
	char *buffer;
	size_t len;

	fp = fopen(filename, "rb");
	if(fp == NULL) {
		fprintf(stderr, "can't open input file\n");
		return NULL;
	}

	fstat(fileno(fp), &s);

	len = (size_t)s.st_size;

	buffer = (char *)calloc(len, sizeof(char));
	if(buffer == NULL) {
		fprintf(stderr, "can't allocate memory for filedata\n");
		return NULL;
	}

	fread(buffer, 1, len, fp);

	fclose(fp);

	mj_jpeg_t *m;

	m = mj_read_jpeg_from_buffer(buffer, len);
	free(buffer);

	if(m == NULL) {
		fprintf(stderr, "reading from buffer failed\n");
		return NULL;
	}

	return m;
}

void mj_destroy_jpeg(mj_jpeg_t *m) {
	fprintf(stderr, "entering %s\n", __FUNCTION__);

	if(m == NULL) {
		return;
	}

	jpeg_destroy_decompress(&m->cinfo);

	m->coef = NULL;

	free(m);

	return;
}

int mj_encode_jpeg_to_buffer(char **buffer, size_t *len, unsigned char *data, int colorspace, J_COLOR_SPACE jpeg_colorspace, mj_sampling_t *s, int width, int height) {
	fprintf(stderr, "entering %s\n", __FUNCTION__);

	struct jpeg_compress_struct cinfo;
	struct mj_jpeg_error_mgr jerr;
	struct mj_jpeg_dest_mgr dest;
	char jpegerrorbuffer[JMSG_LENGTH_MAX];

	cinfo.err = jpeg_std_error(&jerr.pub);
	jerr.pub.error_exit = mj_jpeg_error_exit;
	if(setjmp(jerr.setjmp_buffer)) {
		(*cinfo.err->format_message)((j_common_ptr)&cinfo, jpegerrorbuffer);
		jpeg_destroy_compress(&cinfo);
		if(dest.buf != NULL) {
			free(dest.buf);
		}

		return MJ_ERR;
	}

	jpeg_create_compress(&cinfo);

	cinfo.dest = &dest.pub;
	dest.buf = NULL;
	dest.size = 0;
	dest.pub.init_destination = mj_jpeg_init_destination;
	dest.pub.empty_output_buffer = mj_jpeg_empty_output_buffer;
	dest.pub.term_destination = mj_jpeg_term_destination;

	cinfo.image_width = width;
	cinfo.image_height = height;

	if(colorspace == MJ_COLORSPACE_RGB) {
		cinfo.input_components = 3;
		cinfo.in_color_space = JCS_RGB;
	}
	else if(colorspace == MJ_COLORSPACE_YCC) {
		cinfo.input_components = 3;
		cinfo.in_color_space = JCS_YCbCr;
	}
	else if(colorspace == MJ_COLORSPACE_GRAYSCALE) {
		cinfo.input_components = 1;
		cinfo.in_color_space = JCS_GRAYSCALE;
	}
	else {
		fprintf(stderr, "invalid colorspace\n");
		jpeg_destroy_compress(&cinfo);
		return MJ_ERR;
	}

	jpeg_set_defaults(&cinfo);

	jpeg_set_colorspace(&cinfo, jpeg_colorspace);

	//cinfo.optimize_coding = TRUE;

	if(colorspace == MJ_COLORSPACE_RGB || colorspace == MJ_COLORSPACE_YCC) {
		cinfo.comp_info[0].h_samp_factor = s->samp_factor[0].h_samp_factor;
		cinfo.comp_info[0].v_samp_factor = s->samp_factor[0].v_samp_factor;

		cinfo.comp_info[1].h_samp_factor = s->samp_factor[1].h_samp_factor;;
		cinfo.comp_info[1].v_samp_factor = s->samp_factor[1].v_samp_factor;;

		cinfo.comp_info[2].h_samp_factor = s->samp_factor[2].h_samp_factor;;
		cinfo.comp_info[2].v_samp_factor = s->samp_factor[2].v_samp_factor;;
	}
	else {
		cinfo.comp_info[0].h_samp_factor = s->samp_factor[0].h_samp_factor;;
		cinfo.comp_info[0].v_samp_factor = s->samp_factor[0].v_samp_factor;;
	}

	jpeg_set_quality(&cinfo, 100, TRUE);

	jpeg_start_compress(&cinfo, TRUE);

	int row_stride = cinfo.image_width * cinfo.input_components;

	JSAMPROW row_pointer[1];

	while(cinfo.next_scanline < cinfo.image_height) {
		row_pointer[0] = &data[cinfo.next_scanline * row_stride];
		jpeg_write_scanlines(&cinfo, row_pointer, 1);
	}

	jpeg_finish_compress(&cinfo);
	jpeg_destroy_compress(&cinfo);

	*buffer = (char *)dest.buf;
	*len = dest.size;

	return MJ_OK;
}
