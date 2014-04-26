/*
 * effect.c
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
#include "effect.h"

int modjpeg_effect_grayscale(modjpeg_handle *m) {
	int i, c, k, l;
	jpeg_component_info *component;
	JBLOCKARRAY blocks;
	JCOEFPTR coefs;

	if(m->coef == NULL)
		return -1;

	/* Alle Farbkomponenten auf 0 setzen */
	for(c = 1; c < m->cinfo.num_components; c++) {
		component = &m->cinfo.comp_info[c];

		for(l = 0; l < component->height_in_blocks; l++) {
			blocks = (*m->cinfo.mem->access_virt_barray)((j_common_ptr)&m->cinfo, m->coef[c], l, 1, TRUE);

			for(k = 0; k < component->width_in_blocks; k++) {
				coefs = blocks[0][k];
				for(i = 0; i < DCTSIZE2; i += 4) {
					coefs[i] = 0;
					coefs[i + 1] = 0;
					coefs[i + 2] = 0;
					coefs[i + 3] = 0;
				}
			}
		}
	}

	return 0;
}

int modjpeg_effect_pixelate(modjpeg_handle *m) {
	int i, c, k, l;
	jpeg_component_info *component;
	JBLOCKARRAY blocks;
	JCOEFPTR coefs;

	if(m->coef == NULL)
		return -1;

	/* Bei allen Componenten die AC-Koeffizienten auf 0 setzen */
	for(c = 0; c < m->cinfo.num_components; c++) {
		component = &m->cinfo.comp_info[c];

		for(l = 0; l < component->height_in_blocks; l++) {
			blocks = (*m->cinfo.mem->access_virt_barray)((j_common_ptr)&m->cinfo, m->coef[c], l, 1, TRUE);

			for(k = 0; k < component->width_in_blocks; k++) {
				coefs = blocks[0][k];

				coefs[1] = 0;
				coefs[2] = 0;
				coefs[3] = 0;

				for(i = 4; i < DCTSIZE2; i += 4) {
					coefs[i] = 0;
					coefs[i + 1] = 0;
					coefs[i + 2] = 0;
					coefs[i + 3] = 0;
				}
			}
		}
	}

	return 0;
}

int modjpeg_effect_tint(modjpeg_handle *m, int cb_value, int cr_value) {
	int l, k;
	jpeg_component_info *component;
	JBLOCKARRAY blocks;
	JCOEFPTR coefs;

	if(m->coef == NULL)
		return -1;

	if(m->cinfo.jpeg_color_space != JCS_YCbCr)
		return -1;

	if(cb_value == 0 && cr_value == 0)
		return 0;

	if(cb_value != 0) {
		component = &m->cinfo.comp_info[1];

		for(l = 0; l < component->height_in_blocks; l++) {
			blocks = (*m->cinfo.mem->access_virt_barray)((j_common_ptr)&m->cinfo, m->coef[1], l, 1, TRUE);

			for(k = 0; k < component->width_in_blocks; k++) {
				coefs = blocks[0][k];
				coefs[0] = (coefs[0] * component->quant_table->quantval[0]) + cb_value;

				if(coefs[0] > 2047)
					coefs[0] = 2047;
				else if(coefs[0] < -2047)
					coefs[0] = -2047;

				coefs[0] /= component->quant_table->quantval[0];
			}
		}
	}

	if(cr_value != 0) {
		component = &m->cinfo.comp_info[2];

		for(l = 0; l < component->height_in_blocks; l++) {
			blocks = (*m->cinfo.mem->access_virt_barray)((j_common_ptr)&m->cinfo, m->coef[2], l, 1, TRUE);

			for(k = 0; k < component->width_in_blocks; k++) {
				coefs = blocks[0][k];
				coefs[0] = (coefs[0] * component->quant_table->quantval[0]) + cr_value;

				if(coefs[0] > 2047)
					coefs[0] = 2047;
				else if(coefs[0] < -2047)
					coefs[0] = -2047;

				coefs[0] /= component->quant_table->quantval[0];
			}
		}
	}

	return 0;
}

int modjpeg_effect_luminance(modjpeg_handle *m, int value) {
	int l, k;
	jpeg_component_info *component;
	JBLOCKARRAY blocks;
	JCOEFPTR coefs;

	if(m->coef == NULL)
		return -1;

	component = &m->cinfo.comp_info[0];

	for(l = 0; l < component->height_in_blocks; l++) {
		blocks = (*m->cinfo.mem->access_virt_barray)((j_common_ptr)&m->cinfo, m->coef[0], l, 1, TRUE);

		for(k = 0; k < component->width_in_blocks; k++) {
			coefs = blocks[0][k];
			coefs[0] = (coefs[0] * component->quant_table->quantval[0]) + value;

			if(coefs[0] > 2047)
				coefs[0] = 2047;
			else if(coefs[0] < -2047)
				coefs[0] = -2047;

			coefs[0] /= component->quant_table->quantval[0];
		}
	}

	return 0;
}
