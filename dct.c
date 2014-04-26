/*
 * dct.c
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

#include <math.h>

#include "libmodjpeg.h"
#include "dct.h"

void modjpeg_costable_init(modjpeg *mj) {
	int i, j;

	// Cosinustabelle fuellen
	for(i = 0; i < 8; i++) {
		for(j = 0; j < 4; j++) {
			if((i % 2))		// ungerade
				mj->costable[(i << 2) + j] = cos((M_PI * (2.0 * (float)j + 1.0) / 16.0) + (M_PI * (float)((i - 1) / 2) * (2.0 * (float)j + 1.0) / 8.0));
			else
				mj->costable[(i << 2) + j] = cos(M_PI * (float)(i / 2) * (2.0 * (float)j + 1.0) / 8.0);
		}
	}

	return;
}

void modjpeg_dct_transform_image(modjpeg *mj, modjpeg_image *m) {
	int c, i, j;
	modjpeg_image_component *component;
	modjpeg_image_block block;

	for(c = 0; c < m->num_components; c++) {
		component = m->component[c];

		for(i = 0; i < component->height_in_blocks; i++) {
			for(j = 0; j < component->width_in_blocks; j++) {
				block = component->block[i][j];

				modjpeg_dct_transform_block(mj, block);
			}
		}
	}

	return;		
}

void modjpeg_dct_transform_block(modjpeg *mj, modjpeg_image_block b) {
	int i, j;
	float w[8], r[MODJPEG_BLOCKSIZE2];

	for(j = 0; j < MODJPEG_BLOCKSIZE2; j++)
		b[j] -= 128.0;

	for(j = 0; j < 8; j++) {
		for(i = 0; i < 8; i++) {
			w[0] = modjpeg_1DFDCT(mj, &b[0], j);
			w[1] = modjpeg_1DFDCT(mj, &b[8], j);
			w[2] = modjpeg_1DFDCT(mj, &b[16], j);
			w[3] = modjpeg_1DFDCT(mj, &b[24], j);
			w[4] = modjpeg_1DFDCT(mj, &b[32], j);
			w[5] = modjpeg_1DFDCT(mj, &b[40], j);
			w[6] = modjpeg_1DFDCT(mj, &b[48], j);
			w[7] = modjpeg_1DFDCT(mj, &b[56], j);

			r[(i << 3) + j] = modjpeg_1DFDCT(mj, w, i) * c(i) * c(j);
		}
	}

	for(j = 0; j < MODJPEG_BLOCKSIZE2; j++)
		b[j] = r[j];

	return;
}

float modjpeg_1DFDCT(modjpeg *mj, float *v, int i) {
	float temp;

	if((i % 2)) {	// i ungerade
		temp = (v[0] - v[7]) * mj->costable[(i << 2)];
		temp += (v[1] - v[6]) * mj->costable[(i << 2) + 1];
		temp += (v[2] - v[5]) * mj->costable[(i << 2) + 2];
		temp += (v[3] - v[4]) * mj->costable[(i << 2) + 3];
	}
	else {			// i gerade
		temp = (v[0] + v[7]) * mj->costable[(i << 2)];
		temp += (v[1] + v[6]) * mj->costable[(i << 2) + 1];
		temp += (v[2] + v[5]) * mj->costable[(i << 2) + 2];
		temp += (v[3] + v[4]) * mj->costable[(i << 2) + 3];
	}

	return temp;
}
