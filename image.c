/*
 * image.c
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

#include <stdlib.h>

#include "libmodjpeg.h"
#include "image.h"
#include "jpeg.h"

modjpeg_handle *modjpeg_set_image_file(modjpeg *mj, const char *image) {
	modjpeg_handle *m;

	if(mj == NULL)
		return NULL;

	if(image == NULL)
		return NULL;

	m = modjpeg_set_image(mj, (void *)image, 0, MODJPEG_IMAGE_FILENAME);

	return m;
}

modjpeg_handle *modjpeg_set_image_mem(modjpeg *mj, const char *image, int image_size) {
	modjpeg_handle *m;

	if(mj == NULL)
		return NULL;

	if(image == NULL || image_size <= 0)
		return NULL;

	m = modjpeg_set_image(mj, (void *)image, image_size, MODJPEG_IMAGE_BUFFER);

	return m;
}

modjpeg_handle *modjpeg_set_image_fp(modjpeg *mj, const FILE *image) {
	modjpeg_handle *m;

	if(mj == NULL)
		return NULL;

	if(image == NULL)
		return NULL;

	m = modjpeg_set_image(mj, (void *)image, 0, MODJPEG_IMAGE_FILEPOINTER);

	return m;
}

modjpeg_handle *modjpeg_set_image(modjpeg *mj, const void *image, int image_size, int type) {
	FILE *fp = NULL;
	modjpeg_handle *m = NULL;
	int c;
	struct modjpeg_error_mgr jerr;
	struct modjpeg_src_mgr src;
	jpeg_component_info *component;

	if(image == NULL)
		return NULL;

	if(type == MODJPEG_IMAGE_BUFFER) {
		if(image_size <= 0)
			return NULL;
	}

	m = (modjpeg_handle *)calloc(1, sizeof(modjpeg_handle));
	if(m == NULL)
		return NULL;

	m->mj = mj;

	m->cinfo.err = jpeg_std_error(&jerr.pub);
	jerr.pub.error_exit = modjpeg_error_exit;
	if(setjmp(jerr.setjmp_buffer)) {
		jpeg_destroy_decompress(&m->cinfo);
		if(fp != NULL)
			fclose(fp);

		free(m);

		return NULL;
	}

	jpeg_create_decompress(&m->cinfo);

	if(type == MODJPEG_IMAGE_FILENAME) {
		fp = fopen((char *)image, "rw");
		if(fp == NULL) {
			jpeg_destroy_decompress(&m->cinfo);
			free(m);

			return NULL;
		}

		jpeg_stdio_src(&m->cinfo, fp);
	}
	else if(type == MODJPEG_IMAGE_FILEPOINTER) {
		jpeg_stdio_src(&m->cinfo, (FILE *)image);
	}
	else if(type == MODJPEG_IMAGE_BUFFER) {
		m->cinfo.src = &src.pub;
		src.pub.init_source = modjpeg_init_source;
		src.pub.fill_input_buffer = modjpeg_fill_input_buffer;
		src.pub.skip_input_data = modjpeg_skip_input_data;
		src.pub.resync_to_restart = jpeg_resync_to_restart;
		src.pub.term_source = modjpeg_term_source;

		src.buf = (JOCTET *)image;
		src.size = (size_t)image_size;
	}
	else {
		jpeg_destroy_decompress(&m->cinfo);
		free(m);
		return NULL;
	}

	jpeg_read_header(&m->cinfo, TRUE);

	/* Pruefen, ob das Bild in einem verwenbaren Farbraum ist */
	if(m->cinfo.jpeg_color_space != JCS_YCbCr && m->cinfo.jpeg_color_space != JCS_GRAYSCALE) {
		jpeg_destroy_decompress(&m->cinfo);
		if(fp != NULL)
			fclose(fp);
		free(m);
		return NULL;
	}

	/* Pruefen, ob die Samplingraten symmetrisch sind */
	for(c = 0; c < m->cinfo.num_components; c++) {
		component = &m->cinfo.comp_info[c];

		if(component->h_samp_factor != component->v_samp_factor) {
			jpeg_destroy_decompress(&m->cinfo);
			if(fp != NULL)
				fclose(fp);
			free(m);
			return NULL;
		}
	}

	/* Pruefen, ob die Samplingraten zulaessig sind */
	if(m->cinfo.max_h_samp_factor != 1 && m->cinfo.max_h_samp_factor != 2) {
		jpeg_destroy_decompress(&m->cinfo);
		if(fp != NULL)
			fclose(fp);
		free(m);
		return NULL;
	}

	m->coef = jpeg_read_coefficients(&m->cinfo);

// Koennte heikel sein, den fp hier schon zu schliessen
	if(fp != NULL)
		fclose(fp);

	return m;
}

int modjpeg_unset_image(modjpeg_handle *m) {
	jpeg_destroy_decompress(&m->cinfo);

	m->coef = NULL;

	return 0;
}

int modjpeg_get_image_file(modjpeg_handle *m, const char *fname, int options) {
	int c, size;
	char **buffer = NULL;

	if(m == NULL)
		return -1;

	if(fname == NULL)
		return -1;

	c = modjpeg_get_image(m, (void *)fname, buffer, &size, options, MODJPEG_IMAGE_FILENAME);

	return c;
}

int modjpeg_get_image_mem(modjpeg_handle *m, char **buffer, int *size, int options) {
	int c;

	if(m == NULL)
		return -1;

	c = modjpeg_get_image(m, NULL, buffer, size, options, MODJPEG_IMAGE_BUFFER);

	return c;
}

int modjpeg_get_image_fp(modjpeg_handle *m, const FILE *fp, int options) {
	int c, size;
	char **buffer = NULL;

	if(m == NULL)
		return -1;

	if(fp == NULL)
		return -1;

	c = modjpeg_get_image(m, (void *)fp, buffer, &size, options, MODJPEG_IMAGE_FILEPOINTER);

	return c;
}

int modjpeg_get_image(modjpeg_handle *m, const void *image, char **buffer, int *size, int options, int type) {
	FILE *fp = NULL;
	struct jpeg_compress_struct cinfo;
	jvirt_barray_ptr *dst_coef_arrays;
	struct modjpeg_error_mgr jerr;
	struct modjpeg_dest_mgr dest;
	char jpegerrorbuffer[JMSG_LENGTH_MAX];

	/*
	 * Unseren eigenen Error-Handler setzen, damit die libjpeg
	 * im Fehlerfall keinen exit() macht!
	 */
	cinfo.err = jpeg_std_error(&jerr.pub);
	jerr.pub.error_exit = modjpeg_error_exit;
	if(setjmp(jerr.setjmp_buffer)) {
		(*cinfo.err->format_message)((j_common_ptr)&cinfo, jpegerrorbuffer);
		//printf("%s\n", jpegerrorbuffer);
		jpeg_destroy_compress(&cinfo);
		if(dest.buf != NULL)
			free(dest.buf);
		if(fp != NULL)
			fclose(fp);
		return -1;
	}

	jpeg_create_compress(&cinfo);

	if(type == MODJPEG_IMAGE_FILENAME) {
		fp = fopen((char *)image, "wb");
		if(fp == NULL)
			return -1;

		jpeg_stdio_dest(&cinfo, fp);
	}
	else if(type == MODJPEG_IMAGE_FILEPOINTER) {
		jpeg_stdio_dest(&cinfo, (FILE *)image);
	}
	else if(type == MODJPEG_IMAGE_BUFFER) {
		cinfo.dest = &dest.pub;
		dest.buf = NULL;
		dest.size = 0;
		dest.pub.init_destination = modjpeg_init_destination;
		dest.pub.empty_output_buffer = modjpeg_empty_output_buffer;
		dest.pub.term_destination = modjpeg_term_destination;
	}
	else {
		jpeg_destroy_compress(&cinfo);
		return -1;
	}

	jpeg_copy_critical_parameters(&m->cinfo, &cinfo);

	cinfo.optimize_coding = FALSE;
	if((options & MODJPEG_OPTIMIZE) != 0)
		cinfo.optimize_coding = TRUE;

	dst_coef_arrays = m->coef;

	/* Die neuen Koeffizienten speichern */
	jpeg_write_coefficients(&cinfo, dst_coef_arrays);

	jpeg_finish_compress(&cinfo);
	jpeg_destroy_compress(&cinfo);

	if(type == MODJPEG_IMAGE_BUFFER) {
		*buffer = (char *)dest.buf;
		*size = dest.size;
	}
	else if(type == MODJPEG_IMAGE_FILENAME)
		fclose(fp);

	return 0;
}
