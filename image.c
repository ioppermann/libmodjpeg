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
#include <string.h>
#include <sys/stat.h>

#include "libmodjpeg.h"
#include "image.h"
#include "jpeg.h"

int mj_read_jpeg_from_buffer(mj_jpeg_t *m, const char *buffer, size_t len) {
	if(m == NULL) {
		return MJ_ERR_NULL_DATA;
	}

	if(buffer == NULL || len == 0) {
		return MJ_ERR_NULL_DATA;
	}

	mj_free_jpeg(m);

	struct mj_jpeg_error_mgr jerr;
	struct mj_jpeg_src_mgr src;

	m->cinfo.err = jpeg_std_error(&jerr.pub);
	jerr.pub.error_exit = mj_jpeg_error_exit;
	if(setjmp(jerr.setjmp_buffer)) {
		jpeg_destroy_decompress(&m->cinfo);
		return MJ_ERR_DECODE_JPEG;
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

	// save markers (must happen before jpeg_read_header)
	jpeg_save_markers(&m->cinfo, JPEG_COM, 0xFFFF);

	int i = 0;
	for(i = 0; i < 16; i++) {
		jpeg_save_markers(&m->cinfo, JPEG_APP0 + i, 0xFFFF);
	}

	jpeg_read_header(&m->cinfo, TRUE);

	m->width = m->cinfo.image_width;
	m->height = m->cinfo.image_height;

	switch(m->cinfo.jpeg_color_space) {
		case JCS_GRAYSCALE:
		case JCS_RGB:
		case JCS_YCbCr:
			break;
		default:
			jpeg_destroy_decompress(&m->cinfo);
			free(m);
			return MJ_ERR_UNSUPPORTED_COLORSPACE;
	}

	m->coef = jpeg_read_coefficients(&m->cinfo);

	m->sampling.max_h_samp_factor = m->cinfo.max_h_samp_factor;
	m->sampling.max_v_samp_factor = m->cinfo.max_v_samp_factor;

	m->sampling.h_factor = (m->sampling.max_h_samp_factor * DCTSIZE);
	m->sampling.v_factor = (m->sampling.max_v_samp_factor * DCTSIZE);

	int c;
	jpeg_component_info *component;

	for(c = 0; c < m->cinfo.num_components; c++) {
		component = &m->cinfo.comp_info[c];

		m->sampling.samp_factor[c].h_samp_factor = component->h_samp_factor;
		m->sampling.samp_factor[c].v_samp_factor = component->v_samp_factor;
	}

	return MJ_OK;
}

int mj_read_jpeg_from_file(mj_jpeg_t *m, const char *filename) {
	if(m == NULL) {
		return MJ_ERR_NULL_DATA;
	}

	FILE *fp;
	struct stat s;
	char *buffer;
	size_t len;

	fp = fopen(filename, "rb");
	if(fp == NULL) {
		return MJ_ERR_FILEIO;
	}

	fstat(fileno(fp), &s);

	len = (size_t)s.st_size;

	buffer = (char *)calloc(len + 1, sizeof(char));
	if(buffer == NULL) {
		return MJ_ERR_MEMORY;
	}

	fread(buffer, 1, len, fp);

	fclose(fp);

	int rv;
	rv = mj_read_jpeg_from_buffer(m, buffer, len);
	free(buffer);

	return rv;
}

int mj_write_jpeg_to_buffer(mj_jpeg_t *m, char **buffer, size_t *len, int options) {
	if(m == NULL) {
		return MJ_ERR_NULL_DATA;
	}

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

		return MJ_ERR_ENCODE_JPEG;
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
	else {
		cinfo.optimize_coding = FALSE;
	}

	if((options & MJ_OPTION_PROGRESSIVE) != 0) {
		jpeg_simple_progression(&cinfo);
	}
	else {
		cinfo.scan_info = NULL;
	}

	if((options & MJ_OPTION_ARITHMETRIC) != 0) {
		cinfo.arith_code = TRUE;
	}
	else {
		cinfo.arith_code = FALSE;
	}

	dst_coef_arrays = m->coef;

	// save the new coefficients
	jpeg_write_coefficients(&cinfo, dst_coef_arrays);

	// copy the saved markers
	jpeg_saved_marker_ptr marker;
	for(marker = m->cinfo.marker_list; marker != NULL; marker = marker->next) {
		jpeg_write_marker(&cinfo, marker->marker, marker->data, marker->data_length);
	}

	jpeg_finish_compress(&cinfo);
	jpeg_destroy_compress(&cinfo);

	*buffer = (char *)dest.buf;
	*len = dest.size;

	return MJ_OK;
}

int mj_write_jpeg_to_file(mj_jpeg_t *m, char *filename, int options) {
	FILE *fp;
	char *rebuffer = NULL;
	size_t relen = 0;

	if(m == NULL) {
		return MJ_ERR_NULL_DATA;
	}

	fp = fopen(filename, "wb");
	if(fp == NULL) {
		return MJ_ERR_FILEIO;
	}

	mj_write_jpeg_to_buffer(m, &rebuffer, &relen, options);

	fwrite(rebuffer, 1, relen, fp);
	fclose(fp);

	free(rebuffer);

	return 0;
}

void mj_init_jpeg(mj_jpeg_t *m) {
	if(m == NULL) {
		return;
	}

	memset(m, 0, sizeof(mj_jpeg_t));
}

void mj_free_jpeg(mj_jpeg_t *m) {
	if(m == NULL) {
		return;
	}

	jpeg_destroy_decompress(&m->cinfo);

	mj_init_jpeg(m);

	return;
}

int mj_encode_jpeg_to_buffer(char **buffer, size_t *len, unsigned char *data, int colorspace, J_COLOR_SPACE jpeg_colorspace, mj_sampling_t *s, int width, int height) {
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

		return MJ_ERR_ENCODE_JPEG;
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
		jpeg_destroy_compress(&cinfo);
		return MJ_ERR_UNSUPPORTED_COLORSPACE;
	}

	jpeg_set_defaults(&cinfo);

	jpeg_set_colorspace(&cinfo, jpeg_colorspace);

	cinfo.optimize_coding = FALSE;	// don't optimize
	cinfo.scan_info = NULL;		// don't create progressive output
	cinfo.arith_code = FALSE;	// don't do arithmetric coding

	if(colorspace == MJ_COLORSPACE_RGB || colorspace == MJ_COLORSPACE_YCC) {
		cinfo.comp_info[0].h_samp_factor = s->samp_factor[0].h_samp_factor;
		cinfo.comp_info[0].v_samp_factor = s->samp_factor[0].v_samp_factor;

		cinfo.comp_info[1].h_samp_factor = s->samp_factor[1].h_samp_factor;
		cinfo.comp_info[1].v_samp_factor = s->samp_factor[1].v_samp_factor;

		cinfo.comp_info[2].h_samp_factor = s->samp_factor[2].h_samp_factor;
		cinfo.comp_info[2].v_samp_factor = s->samp_factor[2].v_samp_factor;
	}
	else {
		cinfo.comp_info[0].h_samp_factor = s->samp_factor[0].h_samp_factor;
		cinfo.comp_info[0].v_samp_factor = s->samp_factor[0].v_samp_factor;
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

int mj_decode_jpeg_to_buffer(char **buffer, size_t *len, int *width, int *height, int want_colorspace, const char *filename) {
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
		return MJ_ERR_DECODE_JPEG;
	}

	jpeg_create_decompress(&cinfo);

	fp = fopen(filename, "rb");
	if(fp == NULL) {
		jpeg_destroy_decompress(&cinfo);
		return MJ_ERR_FILEIO;
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
			jpeg_destroy_decompress(&cinfo);
			return MJ_ERR_UNSUPPORTED_COLORSPACE;
	}

	jpeg_start_decompress(&cinfo);

	*width = cinfo.output_width;
	*height = cinfo.output_height;

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

