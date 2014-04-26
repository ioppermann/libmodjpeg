/*
 * read.c
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

#include "libmodjpeg.h"
#include "read.h"
#include "memory.h"
#include "dct.h"
#include "resample.h"
#include "jpeg.h"

/* Liest ein JPEG ein und veranlasst das Resampling */
modjpeg_image *modjpeg_read_image_jpeg(modjpeg *mj, const char *fname, const char *buffer, int size, int options) {
	FILE *fp = NULL;
	int c, i, j, k;
	modjpeg_image *image;
	modjpeg_image_block block;
	struct jpeg_decompress_struct cinfo;
	jvirt_barray_ptr *coef;
	jpeg_component_info *component;
	JBLOCKARRAY blocks;
	JCOEFPTR coefs;
	struct modjpeg_error_mgr jerr;
	struct modjpeg_src_mgr src;
	char jpegerrorbuffer[JMSG_LENGTH_MAX];

	if(fname == NULL && (buffer == NULL || size <= 0))
		return NULL;

	/*
	 * Unseren eigenen Error-Handler setzen, damit die libjpeg
	 * im Fehlerfall keinen exit() macht!
	 */
	cinfo.err = jpeg_std_error(&jerr.pub);
	jerr.pub.error_exit = modjpeg_error_exit;
	if(setjmp(jerr.setjmp_buffer)) {
		(*cinfo.err->format_message)((j_common_ptr)&cinfo, jpegerrorbuffer);
		jpeg_destroy_decompress(&cinfo);
		fclose(fp);
		return NULL;
	}

	jpeg_create_decompress(&cinfo);

	if(fname != NULL) {
		fp = fopen(fname, "rb");
		if(fp == NULL)
			return NULL;

		jpeg_stdio_src(&cinfo, fp);
	}
	else {
		cinfo.src = &src.pub;
		src.pub.init_source = modjpeg_init_source;
		src.pub.fill_input_buffer = modjpeg_fill_input_buffer;
		src.pub.skip_input_data = modjpeg_skip_input_data;
		src.pub.resync_to_restart = jpeg_resync_to_restart;
		src.pub.term_source = modjpeg_term_source;

		src.buf = (JOCTET *)buffer;
		src.size = (size_t)size;
	}

	jpeg_read_header(&cinfo, TRUE);

	/* Pruefen, ob das Bild in einem verwenbaren Farbraum ist */
	if(cinfo.jpeg_color_space != JCS_YCbCr && cinfo.jpeg_color_space != JCS_GRAYSCALE) {
		jpeg_destroy_decompress(&cinfo);
		if(fp != NULL)
			fclose(fp);
		return NULL;
	}

	/* Pruefen, ob die Samplingraten zulaessig sind */
	for(c = 0; c < cinfo.num_components; c++) {
		component = &cinfo.comp_info[c];

		if(component->h_samp_factor != component->v_samp_factor) {
			jpeg_destroy_decompress(&cinfo);
			if(fp != NULL)
				fclose(fp);
			return NULL;
		}
	}

	if(cinfo.max_h_samp_factor != 1 && cinfo.max_h_samp_factor != 2) {
		jpeg_destroy_decompress(&cinfo);
		if(fp != NULL)
			fclose(fp);
		return NULL;
	}

	coef = jpeg_read_coefficients(&cinfo);

	image = modjpeg_alloc_image(mj, cinfo.num_components);

	if(image == NULL) {
		jpeg_destroy_decompress(&cinfo);
		if(fp != NULL)
			fclose(fp);
		return NULL;
	}

	image->image_width = cinfo.image_width;
	image->image_height = cinfo.image_height;
	image->max_h_samp_factor = cinfo.max_h_samp_factor;
	image->max_v_samp_factor = cinfo.max_v_samp_factor;
	image->jpeg_color_space = cinfo.jpeg_color_space;

	for(c = 0; c < image->num_components; c++) {
		component = &cinfo.comp_info[c];

		/* Es werden nur komplette Bloecke genommen, d.h. Bilder werden i.d.R. auf das
		 * naechst kleinere vielfache von 16 zurechtgeschnitten.
		 */
		image->component[c]->width_in_blocks = image->image_width / (image->max_h_samp_factor * DCTSIZE) * component->h_samp_factor;
		image->component[c]->height_in_blocks = image->image_height / (image->max_v_samp_factor * DCTSIZE) * component->v_samp_factor;

		/* Falls der Samplingfaktor von allen Komponenten gleich ist, muss darauf geachtet
		 * werden, dass die Anzahl Bloecke gerade ist, sonst gibt es beim Downsampling
		 * Probleme. Daher sollten Bilder ein vielfaches von 16 in der Breite und Hoehe sein,
		 * damit nichts abgeschnitten wird.
		 */
		if(image->max_h_samp_factor / component->h_samp_factor == 1) {
			if((image->component[c]->width_in_blocks % 2) != 0)
				image->component[c]->width_in_blocks--;

			if((image->component[c]->height_in_blocks % 2) != 0)
				image->component[c]->height_in_blocks--;
		}

		image->component[c]->width_in_sblocks[0] = 0;
		image->component[c]->height_in_sblocks[0] = 0;
		image->component[c]->width_in_sblocks[1] = 0;
		image->component[c]->height_in_sblocks[1] = 0;

		/* Nur wichtig, um zu bestimmen, ob diese Komponente up- oder downsampled werden muss */
		image->component[c]->h_samp_factor = component->h_samp_factor;
		image->component[c]->v_samp_factor = component->v_samp_factor;

		image->component[c]->block = modjpeg_alloc_component_block(mj, image->component[c]->width_in_blocks, image->component[c]->height_in_blocks);
		if(image->component[c]->block == NULL) {
			modjpeg_free_image(mj, image);
			jpeg_destroy_decompress(&cinfo);
			if(fp != NULL)
				fclose(fp);
			return NULL;
		}

		image->component[c]->sblock[0] = NULL;
		image->component[c]->sblock[1] = NULL;

		for(i = 0; i < image->component[c]->height_in_blocks; i++) {
			blocks = (*cinfo.mem->access_virt_barray)((j_common_ptr)&cinfo, coef[c], i, 1, TRUE);

			for(j = 0; j < image->component[c]->width_in_blocks; j++) {
				coefs = blocks[0][j];
				block = image->component[c]->block[i][j];

				for(k = 0; k < DCTSIZE2; k += 4) {
					block[k] = (float)(coefs[k] * component->quant_table->quantval[k]);
					block[k + 1] = (float)(coefs[k + 1] * component->quant_table->quantval[k + 1]);
					block[k + 2] = (float)(coefs[k + 2] * component->quant_table->quantval[k + 2]);
					block[k + 3] = (float)(coefs[k + 3] * component->quant_table->quantval[k + 3]);
				}
			}
		}
	}

	jpeg_destroy_decompress(&cinfo);

	if(fp != NULL)
		fclose(fp);

	/* Das Bild resamplen */
	modjpeg_resample_image(mj, image);

	if((options & MODJPEG_MASK) != 0)
		modjpeg_prepare_mask(image, options);
	
	return image;
}

modjpeg_image *modjpeg_read_image_raw(modjpeg *mj, const char *buffer, int width, int height, int buffer_components, int type, int options) {
	int c, i, j, k, p;
	modjpeg_image *image;
	modjpeg_image_block block;

	if(buffer == NULL || height < 16 || width < 16)
		return NULL;

	if(type == MODJPEG_RAWALPHA)
		image = modjpeg_alloc_image(mj, 1);
	else
		image = modjpeg_alloc_image(mj, 3);

	if(image == NULL)
		return NULL;

	image->image_width = width;
	image->image_height =  height;
	image->max_h_samp_factor = 1;
	image->max_v_samp_factor = 1;

	if(type == MODJPEG_RAWALPHA)
		image->jpeg_color_space = JCS_GRAYSCALE;
	else
		image->jpeg_color_space = JCS_YCbCr;

	for(c = 0; c < image->num_components; c++) {
		/* Es werden nur komplette Bloecke genommen */
		image->component[c]->width_in_blocks = image->image_width / MODJPEG_BLOCKSIZE;
		image->component[c]->height_in_blocks = image->image_height / MODJPEG_BLOCKSIZE;

		/* Der Samplingfaktor von allen Komponenten ist gleich. Es muss darauf geachtet
		 * werden, dass die Anzahl Bloecke gerade ist, sonst gibt es beim Downsampling
		 * Probleme. Daher sollten Bilder ein vielfaches von 16 in der Breite und Hoehe sein,
		 * damit nichts abgeschnitten wird.
		 */
		if((image->component[c]->width_in_blocks % 2) != 0)
			image->component[c]->width_in_blocks--;

		if((image->component[c]->height_in_blocks % 2) != 0)
			image->component[c]->height_in_blocks--;

		image->component[c]->width_in_sblocks[0] = 0;
		image->component[c]->height_in_sblocks[0] = 0;
		image->component[c]->width_in_sblocks[1] = 0;
		image->component[c]->height_in_sblocks[1] = 0;

		/* Nur wichtig, um zu bestimmen, ob diese Komponente up- oder downsampled werden muss */
		image->component[c]->h_samp_factor = 1;
		image->component[c]->v_samp_factor = 1;

		image->component[c]->block = modjpeg_alloc_component_block(mj, image->component[c]->width_in_blocks, image->component[c]->height_in_blocks);
		if(image->component[c]->block == NULL) {
			modjpeg_free_image(mj, image);
			return NULL;
		}

		image->component[c]->sblock[0] = NULL;
		image->component[c]->sblock[1] = NULL;

		for(i = 0; i < image->component[c]->height_in_blocks; i++) {
			for(j = 0; j < image->component[c]->width_in_blocks; j++) {
				block = image->component[c]->block[i][j];

				for(k = 0; k < MODJPEG_BLOCKSIZE2; k++) {
					p = ((i << 3) + (k >> 3)) * (image->image_width * buffer_components) + ((j << 3) + (k % 8)) * buffer_components;

					if(type == MODJPEG_RAWALPHA)
						block[k] = (float)(unsigned char)buffer[p + (buffer_components - 1)];
					else
						block[k] = (float)(unsigned char)buffer[p + c];
				}
			}
		}
	}

	if((options & MODJPEG_RGB2YCbCr) != 0)
		modjpeg_rgb2ycbcr(image);

	modjpeg_dct_transform_image(mj, image);

	/* Das Bild resamplen */
	modjpeg_resample_image(mj, image);

	if((options & MODJPEG_MASK) != 0)
		modjpeg_prepare_mask(image, options);

	return image;
}

void modjpeg_rgb2ycbcr(modjpeg_image *m) {
	int i, j, k;
	modjpeg_image_block r, g, b;
	float y, u, v;
/*
Y   =     0.299  R + 0.587  G + 0.114  B
Cb  =   - 0.1687 R - 0.3313 G + 0.5    B + 128
Cr  =     0.5    R - 0.4187 G - 0.0813 B + 128
*/
	/* Es sind immer drei Farbkomponenten und gleiche Samplings */

	for(i = 0; i < m->component[0]->height_in_blocks; i++) {
		for(j = 0; j < m->component[0]->width_in_blocks; j++) {
			r = m->component[0]->block[i][j];
			g = m->component[1]->block[i][j];
			b = m->component[2]->block[i][j];

			for(k = 0; k < MODJPEG_BLOCKSIZE2; k++) {
				y =  0.299  * r[k] + 0.587  * g[k] + 0.114  * b[k];
				u = -0.1687 * r[k] - 0.3313 * g[k] + 0.5    * b[k] + 128.0;
				v =  0.5    * r[k] - 0.4187 * g[k] - 0.0813 * b[k] + 128.0;

				r[k] = y;
				g[k] = u;
				b[k] = v;
			}
		}
	}

	return;
}

/* Die Maske vorbereiten:
 * Grauwert verschieben und so viel wie moeglich von der Faltung im Voraus berechnen
 */
void modjpeg_prepare_mask(modjpeg_image *mask, int options)
{
	int i, j, k, l;
	float shift;
	modjpeg_image_block block;

	/*
	 * Die Y-Komponent hat im Farbraum Werte von -128 bis 127, im Frequenzraum Werte
	 * Werte von -1024 bis 1023. Die Maske sollte aber Werte im Bereich [0, 1] haben (Farbraum),
	 * also muss im Frequenzraum ein Shift um 1024 gemacht werden. Die Skalierung
	 * nach [0, 1] erfolgt durch die Division mit 255. Fuer das Wasserzeichen wird der
	 * Shift nicht gemacht, damit die negativen Wert zu einer Abdunklung des Bildes
	 * fuehren (kombiniert mit einem vollstaendig weissen Bild).
	 */
	shift = 1024.0;
	if((options & MODJPEG_NOSHIFT) != 0)
		shift = 0.0;

	for(l = 0; l < 2; l++) {
		/* Fuer die Maske wird nur die Helligkeitskomponente beruecksichtigt */
		mask->component[0]->height_in_blocks = mask->component[0]->height_in_sblocks[l];
		mask->component[0]->width_in_blocks = mask->component[0]->width_in_sblocks[l];

		mask->component[0]->block = mask->component[0]->sblock[l];

		for(i = 0; i < mask->component[0]->height_in_blocks; i++) {
			for(j = 0; j < mask->component[0]->width_in_blocks; j++) {
				block = mask->component[0]->block[i][j];

				block[0] += shift;

				/*
				// w'(j, i) = w(j, i) * 1/255 * c(i) * c(j) * 1/4
				// Der Faktor 1/4 kommt von den V(i) und V(j)
				// => 1/255 * 1/4 = 1/1020
				*/
				block[0] *= (0.3535534 * 0.3535534 / 1020.0);
				block[1] *= (0.3535534 * 0.5 / 1020.0);
				block[2] *= (0.3535534 * 0.5 / 1020.0);
				block[3] *= (0.3535534 * 0.5 / 1020.0);
				block[4] *= (0.3535534 * 0.5 / 1020.0);
				block[5] *= (0.3535534 * 0.5 / 1020.0);
				block[6] *= (0.3535534 * 0.5 / 1020.0);
				block[7] *= (0.3535534 * 0.5 / 1020.0);

				for(k = 1; k < 8; k++) {
					block[(k << 3)]     *= (0.5 * 0.3535534 / 1020.0);
					block[(k << 3) + 1] *= (0.5 * 0.5 / 1020.0);
					block[(k << 3) + 2] *= (0.5 * 0.5 / 1020.0);
					block[(k << 3) + 3] *= (0.5 * 0.5 / 1020.0);
					block[(k << 3) + 4] *= (0.5 * 0.5 / 1020.0);
					block[(k << 3) + 5] *= (0.5 * 0.5 / 1020.0);
					block[(k << 3) + 6] *= (0.5 * 0.5 / 1020.0);
					block[(k << 3) + 7] *= (0.5 * 0.5 / 1020.0);
				}
			}
		}
	}

	mask->component[0]->height_in_blocks = 0;
	mask->component[0]->width_in_blocks = 0;

	mask->component[0]->block = NULL;

	return;
}

