/*
 * logo.c
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
#include "logo.h"
#include "read.h"
#include "memory.h"
#include "convolve.h"
#include "resample.h"

int modjpeg_set_logo_file(modjpeg *mj, const char *logo, const char *mask) {
	int lid;

	if(mj == NULL)
		return -1;

	if(logo == NULL)
		return -1;

	lid = modjpeg_set_logo(mj, logo, NULL, 0, mask, NULL, 0, NULL, 0, 0, 0);

	return lid;
}

int modjpeg_set_logo_mem(modjpeg *mj, const char *logo, int logo_size, const char *mask, int mask_size) {
	int lid;

	if(mj == NULL)
		return -1;

	if(logo == NULL || logo_size <= 0)
		return -1;

	lid = modjpeg_set_logo(mj, NULL, logo, logo_size, NULL, mask, mask_size, NULL, 0, 0, 0);

	return lid;
}

int modjpeg_set_logo_raw(modjpeg *mj, const char *logo, int width, int height, int type) {
	int lid;

	if(mj == NULL)
		return -1;

	if(logo == NULL || width < 16 || height < 16)
		return -1;

	lid = modjpeg_set_logo(mj, NULL, NULL, 0, NULL, NULL, 0, logo, width, height, type);

	return lid;
}

int modjpeg_set_logo(modjpeg *mj, const char *logo_file, const char *logo_buffer, int logo_size, const char *mask_file, const char *mask_buffer, int mask_size, const char *logo_raw, int width, int height, int type) {
	int i, components, options, lid;
	modjpeg_image **logos, **masks;
	modjpeg_image *image_logo, *image_mask;

	image_logo = NULL;
	image_mask = NULL;

	options = MODJPEG_NOOP;

	if(logo_file == NULL && logo_buffer == NULL) {
		switch(type) {
			case MODJPEG_RGB:
				components = 3;
				options |= MODJPEG_RGB2YCbCr;
				break;
			case MODJPEG_RGBA:
				components = 4;
				options |= MODJPEG_RGB2YCbCr;
				break;
			case MODJPEG_YCbCr:
				components = 3;
				break;
			case MODJPEG_YCbCrA:
				components = 4;
				break;
			default:
				return -1;
		}

		image_logo = modjpeg_read_image_raw(mj, logo_raw, width, height, components, MODJPEG_RAWCOLOR, options);
	}
	else
		image_logo = modjpeg_read_image_jpeg(mj, logo_file, logo_buffer, logo_size, options);

	if(image_logo == NULL)
		return -1;

	if(mask_file != NULL || mask_buffer != NULL || type == MODJPEG_RGBA || type == MODJPEG_YCbCrA) {
		if(mask_file != NULL || mask_buffer != NULL)
			image_mask = modjpeg_read_image_jpeg(mj, mask_file, mask_buffer, mask_size, MODJPEG_MASK);
		else
			image_mask = modjpeg_read_image_raw(mj, logo_raw, width, height, 4, MODJPEG_RAWALPHA, MODJPEG_MASK);

		if(image_mask == NULL) {
			modjpeg_free_image(mj, image_logo);
			return -1;
		}
	}

	logos = (modjpeg_image **)calloc(mj->clogos + 1, sizeof(modjpeg_image *));
	masks = (modjpeg_image **)calloc(mj->clogos + 1, sizeof(modjpeg_image *));
	if(logos == NULL || masks == NULL) {
		if(logos != NULL)
			free(logos);

		if(masks != NULL)
			free(masks);

		modjpeg_free_image(mj, image_logo);
		modjpeg_free_image(mj, image_mask);

		return -1;
	}

	for(i = 0; i < mj->clogos; i++) {
		logos[i] = mj->logos[i];
		masks[i] = mj->masks[i];
	}

	if(mj->logos != NULL)
		free(mj->logos);

	if(mj->masks != NULL)
		free(mj->masks);

	mj->clogos++;

	mj->logos = logos;
	mj->masks = masks;

	lid = mj->clogos - 1;

	mj->logos[lid] = image_logo;
	mj->masks[lid] = image_mask;

	return lid;
}

int modjpeg_unset_logo(modjpeg *mj, int lid) {
	if(mj == NULL)
		return -1;

	if(lid < 0)
		return -1;

	if(lid >= mj->clogos)
		return -1;

	modjpeg_free_image(mj, mj->logos[lid]);
	modjpeg_free_image(mj, mj->masks[lid]);

	mj->logos[lid] = NULL;
	mj->masks[lid] = NULL;

	return 0;
}

int modjpeg_unset_logo_all(modjpeg *mj) {
	int i;

	for(i = 0; i < mj->clogos; i++)
		modjpeg_unset_logo(mj, i);

	return 0;
}

int modjpeg_add_logo(modjpeg_handle *m, int lid, int position) {
	int r;

	if(m == NULL)
		return -1;

	if(m->coef == NULL)
		return -1;

	if(lid < 0)
		return -1;

	if(lid >= m->mj->clogos)
		return -1;

	if(m->mj->logos[lid] == NULL)
		return -1;

	if(m->mj->masks[lid] == NULL)
		r = modjpeg_add_logo_nomask(m, lid, position);
	else
		r = modjpeg_add_logo_mask(m, lid, position);

	return r;
}

/* Ein Logo ohne Maske hinzufuegen */
int modjpeg_add_logo_nomask(modjpeg_handle *m, int lid, int position) {
	int c, k, l, i;
	int logo_width_offset, logo_height_offset, h_blocks, v_blocks;
	modjpeg_image *logo;
	modjpeg_image_component *component_logo;
	modjpeg_image_block coefs_logo;
	jpeg_component_info *component;
	JBLOCKARRAY blocks;
	JCOEFPTR coefs;

	logo = m->mj->logos[lid];

	/* Das Logo sollte nicht groesser als das Bild sein */
	if(logo->image_width > m->cinfo.image_width || logo->image_height > m->cinfo.image_height)
		return -1;

	/* Die Anzahl der Komponenten muss uebereinstimmen (vorerst) */
	if(logo->num_components != m->cinfo.num_components)
		return -1;

	/* Der Farbraum muss der selbe sein (vorerst) */
	if(logo->jpeg_color_space != m->cinfo.jpeg_color_space)
		return -1;

	/*
	 * Die effektive Dimension des Bildes in Bloecken berechnen
	 * und darauf achten, dass nicht zu viel vom Logo in Bloecken
	 * landet, die nicht sichtbar sind. Heuristik.
	 */
	h_blocks = m->cinfo.image_width / (m->cinfo.max_h_samp_factor * DCTSIZE);
	if((h_blocks * m->cinfo.max_h_samp_factor * DCTSIZE) < (m->cinfo.image_width - (m->cinfo.max_h_samp_factor * DCTSIZE / 2)))
		h_blocks++;

	v_blocks = m->cinfo.image_height / (m->cinfo.max_v_samp_factor * DCTSIZE);
	if((v_blocks * m->cinfo.max_v_samp_factor * DCTSIZE) < (m->cinfo.image_height - (m->cinfo.max_v_samp_factor * DCTSIZE / 2)))
		v_blocks++;

	for(c = 0; c < logo->num_components; c++) {
		component = &m->cinfo.comp_info[c];
		component_logo = logo->component[c];

		/* Das noetige Sampling waehlen */
		SELECT_SAMPLING(component_logo, m->cinfo.max_h_samp_factor / component->h_samp_factor);

		/* Die Offsets fuer das Logo berechnen */
		if((position & MODJPEG_TOP) != 0)
			logo_height_offset = 0;
		else
			logo_height_offset = (component->v_samp_factor * v_blocks) - component_logo->height_in_blocks;

		if((position & MODJPEG_LEFT) != 0)
			logo_width_offset = 0;
		else
			logo_width_offset = (component->h_samp_factor * h_blocks) - component_logo->width_in_blocks;

		/* Die Werte des Logos in das Bild kopieren */
		for(l = 0; l < component_logo->height_in_blocks; l++) {
			blocks = (*m->cinfo.mem->access_virt_barray)((j_common_ptr)&m->cinfo, m->coef[c], logo_height_offset + l, 1, TRUE);

			for(k = 0; k < component_logo->width_in_blocks; k++) {
				coefs_logo = component_logo->block[l][k];
				coefs = blocks[0][logo_width_offset + k];

				for(i = 0; i < DCTSIZE2; i += 4) {
					coefs[i] = (int)coefs_logo[i] / component->quant_table->quantval[i];
					coefs[i + 1] = (int)coefs_logo[i + 1] / component->quant_table->quantval[i + 1];
					coefs[i + 2] = (int)coefs_logo[i + 2] / component->quant_table->quantval[i + 2];
					coefs[i + 3] = (int)coefs_logo[i + 3] / component->quant_table->quantval[i + 3];
				}
			}
		}
	}

	return 0;
}

/* Ein Logo mit Maske hinzufuegen */
int modjpeg_add_logo_mask(modjpeg_handle *m, int lid, int position) {
	int c, k, l, i;
	int logo_width_offset, logo_height_offset, h_blocks, v_blocks;
	float x[MODJPEG_BLOCKSIZE2], y[MODJPEG_BLOCKSIZE2];
	modjpeg_image *logo, *mask;
	modjpeg_image_component *component_logo, *component_mask;
	modjpeg_image_block coefs_logo, coefs_mask;
	jpeg_component_info *component;
	JBLOCKARRAY blocks;
	JCOEFPTR coefs;

	logo = m->mj->logos[lid];
	mask = m->mj->masks[lid];

	/* Das Logo sollte nicht groesser als das Bild sein */
	if(logo->image_width > m->cinfo.image_width || logo->image_height > m->cinfo.image_height)
		return -1;

	/* Die Anzahl der Komponenten muss uebereinstimmen (vorerst) */
	if(logo->num_components != m->cinfo.num_components)
		return -1;

	/* Der Farbraum muss der selbe sein (vorerst) */
	if(logo->jpeg_color_space != m->cinfo.jpeg_color_space)
		return -1;

	/* Die Maske muss genauso gross sein wie das Logo */
	if(logo->image_width != mask->image_width || logo->image_height != mask->image_height)
		return -1;

	/*
	 * Die effektive Dimension des Bildes in Bloecken berechnen
	 * und darauf achten, dass nicht zu viel vom Logo in Bloecken
	 * landet, die nicht sichtbar sind. Heursitik.
	 */
	h_blocks = m->cinfo.image_width / (m->cinfo.max_h_samp_factor * DCTSIZE);
	if((h_blocks * m->cinfo.max_h_samp_factor * DCTSIZE) < (m->cinfo.image_width - (m->cinfo.max_h_samp_factor * DCTSIZE / 2)))
		h_blocks++;

	v_blocks = m->cinfo.image_height / (m->cinfo.max_v_samp_factor * DCTSIZE);
	if((v_blocks * m->cinfo.max_v_samp_factor * DCTSIZE) < (m->cinfo.image_height - (m->cinfo.max_v_samp_factor * DCTSIZE / 2)))
		v_blocks++;

	/* Von der Maske wird nur die Helligkeitskomponente verwendet */
	component_mask = mask->component[0];

	for(c = 0; c < logo->num_components; c++) {
		component = &m->cinfo.comp_info[c];
		component_logo = logo->component[c];

		/* Das noetige Sampling waehlen */
		SELECT_SAMPLING(component_logo, m->cinfo.max_h_samp_factor / component->h_samp_factor);
		SELECT_SAMPLING(component_mask, m->cinfo.max_h_samp_factor / component->h_samp_factor);

		/* Die Offsets fuer das Logo und die Maske berechnen */
		if((position & MODJPEG_TOP) != 0)
			logo_height_offset = 0;
		else
			logo_height_offset = (component->v_samp_factor * v_blocks) - component_logo->height_in_blocks;

		if((position & MODJPEG_LEFT) != 0)
			logo_width_offset = 0;
		else
			logo_width_offset = (component->h_samp_factor * h_blocks) - component_logo->width_in_blocks;

		/* Die Werte des Originalbildes und des Logos anhand der Maske kombinieren */
		for(l = 0; l < component_logo->height_in_blocks; l++) {
			blocks = (*m->cinfo.mem->access_virt_barray)((j_common_ptr)&m->cinfo, m->coef[c], logo_height_offset + l, 1, TRUE);

			for(k = 0; k < component_logo->width_in_blocks; k++) {
				coefs_logo = component_logo->block[l][k];
				coefs_mask = component_mask->block[l][k];
				coefs = blocks[0][logo_width_offset + k];

				// x = x0 - x1
				for(i = 0; i < DCTSIZE2; i += 4) {
					x[i] = coefs_logo[i] - (float)(coefs[i] * component->quant_table->quantval[i]);
					x[i + 1] = coefs_logo[i + 1] - (float)(coefs[i + 1] * component->quant_table->quantval[i + 1]);
					x[i + 2] = coefs_logo[i + 2] - (float)(coefs[i + 2] * component->quant_table->quantval[i + 2]);
					x[i + 3] = coefs_logo[i + 3] - (float)(coefs[i + 3] * component->quant_table->quantval[i + 3]);
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
				for(i = 0; i < DCTSIZE2; i += 4) {
					coefs[i] = ((int)y[i] + (coefs[i] * component->quant_table->quantval[i])) / component->quant_table->quantval[i];
					coefs[i + 1] = ((int)y[i + 1] + (coefs[i + 1] * component->quant_table->quantval[i + 1])) / component->quant_table->quantval[i + 1];
					coefs[i + 2] = ((int)y[i + 2] + (coefs[i + 2] * component->quant_table->quantval[i + 2])) / component->quant_table->quantval[i + 2];
					coefs[i + 3] = ((int)y[i + 3] + (coefs[i + 3] * component->quant_table->quantval[i + 3])) / component->quant_table->quantval[i + 3];
				}
			}
		}
	}

	return 0;
}
