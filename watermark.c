/*
 * watermark.c
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
#include <string.h>

#include "libmodjpeg.h"
#include "watermark.h"
#include "memory.h"
#include "convolve.h"
#include "resample.h"
#include "read.h"

int modjpeg_set_watermark_file(modjpeg *mj, const char *watermark) {
	int wid;

	if(mj == NULL)
		return -1;

	if(watermark == NULL)
		return -1;

	wid = modjpeg_set_watermark(mj, watermark, NULL, 0, NULL, 0, 0, 0);

	return wid;
}

int modjpeg_set_watermark_mem(modjpeg *mj, const char *watermark, int watermark_size) {
	int wid;

	if(mj == NULL)
		return -1;

	if(watermark == NULL || watermark_size <= 0)
		return -1;

	wid = modjpeg_set_watermark(mj, NULL, watermark, watermark_size, NULL, 0, 0, 0);

	return wid;
}

int modjpeg_set_watermark_raw(modjpeg *mj, const char *watermark, int width, int height, int type) {
	int wid;

	if(mj == NULL)
		return -1;

	if(watermark == NULL || width < 16 || height < 16)
		return -1;

	wid = modjpeg_set_watermark(mj, NULL, NULL, 0, watermark, width, height, type);

	return wid;
}

int modjpeg_set_watermark(modjpeg *mj, const char *watermark_file, const char *watermark_buffer, int watermark_size, const char *watermark_raw, int width, int height, int type) {
	int i, options, wid;
	modjpeg_image **watermarks;
	modjpeg_image *image_watermark;

	image_watermark = NULL;

	options = MODJPEG_MASK | MODJPEG_NOSHIFT;

	if(watermark_file == NULL && watermark_buffer == NULL) {
		if(type != MODJPEG_Y)
			return -1;

		image_watermark = modjpeg_read_image_raw(mj, watermark_raw, width, height, 1, MODJPEG_RAWALPHA, options);
	}
	else
		image_watermark = modjpeg_read_image_jpeg(mj, watermark_file, watermark_buffer, watermark_size, options);

	if(image_watermark == NULL)
		return -1;

	watermarks = (modjpeg_image **)calloc(mj->cwatermarks + 1, sizeof(modjpeg_image *));
	if(watermarks == NULL) {
		modjpeg_free_image(mj, image_watermark);

		return -1;
	}

	for(i = 0; i < mj->cwatermarks; i++)
		watermarks[i] = mj->watermarks[i];

	if(mj->watermarks != NULL)
		free(mj->watermarks);

	mj->cwatermarks++;

	mj->watermarks = watermarks;

	wid = mj->cwatermarks - 1;

	mj->watermarks[wid] = image_watermark;

	return wid;
}

int modjpeg_unset_watermark(modjpeg *mj, int wid) {
	if(mj == NULL)
		return -1;

	if(wid < 0)
		return -1;

	if(wid >= mj->cwatermarks)
		return -1;

	modjpeg_free_image(mj, mj->watermarks[wid]);

	mj->watermarks[wid] = NULL;

	return 0;
}

int modjpeg_unset_watermark_all(modjpeg *mj) {
	int i;

	for(i = 0; i < mj->cwatermarks; i++)
		modjpeg_unset_watermark(mj, i);

	return 0;
}

/* Ein Wasserzeichen hinzufuegen */
int modjpeg_add_watermark(modjpeg_handle *m, int wid) {
	int l, k, i;
	int watermark_width_offset, watermark_height_offset, h_samp, v_samp;
	float x[MODJPEG_BLOCKSIZE2], y[MODJPEG_BLOCKSIZE2];
	modjpeg_image *watermark;
	modjpeg_image_component *component_mask;
	modjpeg_image_block coefs_mask;
	jpeg_component_info *component;
	JBLOCKARRAY blocks;
	JCOEFPTR coefs;

	if(m == NULL)
		return -1;

	if(m->coef == NULL)
		return -1;

	if(wid < 0)
		return -1;

	if(wid >= m->mj->cwatermarks)
		return -1;

	if(m->mj->watermarks[wid] == NULL)
		return -1;

	watermark = m->mj->watermarks[wid];

	/* Das Wasserzeichen sollte nicht groesser als das Bild sein */
	if(watermark->image_width > m->cinfo.image_width || watermark->image_height > m->cinfo.image_height)
		return -1;

	/*
	 * Anzahl Komponenten und der Farbraum sind egal, da nur die
	 * Helligkeitskomponente verwendet wird. Die Farbraeume sind
	 * a-priori auf YCbCr und Grayscale festgelegt.
	 */

	/* Nur die Helligkeitskomponente veraendern */
	component = &m->cinfo.comp_info[0];
	component_mask = watermark->component[0];

	SELECT_SAMPLING(component_mask, m->cinfo.max_h_samp_factor / component->h_samp_factor);

	h_samp = m->cinfo.max_h_samp_factor / component->h_samp_factor;
	v_samp = h_samp;

	/* Das Wasserzeichen in der Mitte platzieren */
	watermark_width_offset = (m->cinfo.image_width - watermark->image_width) / (2 * h_samp * DCTSIZE);
	watermark_height_offset = (m->cinfo.image_height - watermark->image_height) / (2 * v_samp * DCTSIZE);

	for(l = 0; l < component_mask->height_in_blocks; l++) {
		blocks = (*m->cinfo.mem->access_virt_barray)((j_common_ptr)&m->cinfo, m->coef[0], watermark_height_offset + l, 1, TRUE);

		for(k = 0; k < component_mask->width_in_blocks; k++) {
			coefs_mask = component_mask->block[l][k];
			coefs = blocks[0][watermark_width_offset + k];

			// x = x0 - x1 (x0 = weiss)
			x[0] = 1023.0 - (float)(coefs[0] * component->quant_table->quantval[0]);
			x[1] = (float)(-coefs[1] * component->quant_table->quantval[1]);
			x[2] = (float)(-coefs[2] * component->quant_table->quantval[2]);
			x[3] = (float)(-coefs[3] * component->quant_table->quantval[3]);

			for(i = 4; i < DCTSIZE2; i += 4) {
				x[i] = (float)(-coefs[i] * component->quant_table->quantval[i]);
				x[i + 1] = (float)(-coefs[i + 1] * component->quant_table->quantval[i + 1]);
				x[i + 2] = (float)(-coefs[i + 2] * component->quant_table->quantval[i + 2]);
				x[i + 3] = (float)(-coefs[i + 3] * component->quant_table->quantval[i + 3]);
			}

			memset(y, 0, MODJPEG_BLOCKSIZE2 * sizeof(float));

			// y' = w * x (Faltung)
			for(i = 0; i < DCTSIZE; i++) {
				modjpeg_convolve_add(x, y, coefs_mask[(i << 3)], i, 0);
				modjpeg_convolve_add(x, y, coefs_mask[(i << 3) + 1], i, 1);
				modjpeg_convolve_add(x, y, coefs_mask[(i << 3) + 2], i, 2);
				modjpeg_convolve_add(x, y, coefs_mask[(i << 3) + 3], i, 3);
				modjpeg_convolve_add(x, y, coefs_mask[(i << 3) + 4], i, 4);
				modjpeg_convolve_add(x, y, coefs_mask[(i << 3) + 5], i, 5);
				modjpeg_convolve_add(x, y, coefs_mask[(i << 3) + 6], i, 6);
				modjpeg_convolve_add(x, y, coefs_mask[(i << 3) + 7], i, 7);
			}

			// y = x1 + y'
			for(i = 0; i < DCTSIZE2; i++) {
				coefs[i] = (int)y[i] + (coefs[i] * component->quant_table->quantval[i]);

				if(coefs[i] > 1023)
					coefs[i] = 1023;
				else if(coefs[i] < -1024)
					coefs[i] = -1024;

				coefs[i] /= component->quant_table->quantval[i];

/*
				coefs[i] = ((int)y[i] + (coefs[i] * component->quant_table->quantval[i])) / component->quant_table->quantval[i];
				coefs[i + 1] = ((int)y[i + 1] + (coefs[i + 1] * component->quant_table->quantval[i + 1])) / component->quant_table->quantval[i + 1];
				coefs[i + 2] = ((int)y[i + 2] + (coefs[i + 2] * component->quant_table->quantval[i + 2])) / component->quant_table->quantval[i + 2];
				coefs[i + 3] = ((int)y[i + 3] + (coefs[i + 3] * component->quant_table->quantval[i + 3])) / component->quant_table->quantval[i + 3];
*/
			}
		}
	}
 
	return 0;
}
