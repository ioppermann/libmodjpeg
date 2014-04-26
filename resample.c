/*
 * resample.c
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

#include <string.h>
#include <math.h>

#include "libmodjpeg.h"
#include "resample.h"
#include "memory.h"
#include "matrix.h"

/* Initialisiert die Matrizen, die fuer das Resampling notwendig sind */
int modjpeg_resample_init(modjpeg *mj) {
	int i, j;
	float t[MODJPEG_BLOCKSIZE2], tst[MODJPEG_BLOCKSIZE2], t1[MODJPEG_BLOCKSIZE2], t2[MODJPEG_BLOCKSIZE2];

	memset(t, 0, MODJPEG_BLOCKSIZE2 * sizeof(float));
	memset(tst, 0, MODJPEG_BLOCKSIZE2 * sizeof(float));
	memset(t1, 0, MODJPEG_BLOCKSIZE2 * sizeof(float));
	memset(t2, 0, MODJPEG_BLOCKSIZE2 * sizeof(float));

	for(i = 0; i < 8; i++) {
		for(j = 0; j < 8; j++) {
			if(i == 0)
				t[(i << 3) + j] = 1.0 / (2.0 * M_SQRT2);
			else
				t[(i << 3) + j] = 0.5 * cos(M_PI * (float)i * (2.0 * (float)j + 1.0) / 16.0);
		}
	}

	for(i = 0; i < 4; i++) {
		for(j = 0; j < 4; j++) {
			if(i == 0)
				tst[(j << 3) + i] = 0.5;
			else
				tst[(j << 3) + i] = cos(M_PI * (float)i * (2.0 * (float)j + 1.0) / 8.0) / M_SQRT2;
		}
	}

	// t1 = tl * tst
	modjpeg_matrix_mul(t1, t, 8, 4, tst, 4, 4);

	// tl -> tr
	for(i = 0; i < 8; i++) {
		for(j = 0; j < 4; j++)
			t[(i << 3) + j] = t[(i << 3) + j + 4];
	}

	// t2 = tr * tst
	modjpeg_matrix_mul(t2, t, 8, 4, tst, 4, 4);

	modjpeg_matrix_add(mj->resamp_c, t1, t2, 8, 4);
	modjpeg_matrix_sub(mj->resamp_d, t1, t2, 8, 4);

	modjpeg_matrix_add(mj->resamp_e, t1, t2, 8, 4);
	modjpeg_matrix_sub(mj->resamp_f, t1, t2, 8, 4);

	for(i = 0; i < 8; i++) {
		for(j = 0; j < 4; j++) {
			mj->resamp_c[(i << 3) + j] /= (2.0 * M_SQRT2);
			mj->resamp_d[(i << 3) + j] /= (2.0 * M_SQRT2);
			mj->resamp_e[(i << 3) + j] *= (M_SQRT2 / 2.0);
			mj->resamp_f[(i << 3) + j] *= (M_SQRT2 / 2.0);
		}
	}

	for(i = 0; i < 8; i++) {
		for(j = 0; j < 8; j++) {
			mj->resamp_ct[(j << 3) + i] = mj->resamp_c[(i << 3) + j];
			mj->resamp_dt[(j << 3) + i] = mj->resamp_d[(i << 3) + j];
			mj->resamp_et[(j << 3) + i] = mj->resamp_e[(i << 3) + j];
			mj->resamp_ft[(j << 3) + i] = mj->resamp_f[(i << 3) + j];
		}
	}

	return 0;
}

int modjpeg_resample_image(modjpeg *mj, modjpeg_image *image) {
	int i, sampling;
	modjpeg_image_component *component;

	for(i = 0; i < image->num_components; i++) {
		component = image->component[i];

		sampling = (image->max_h_samp_factor / component->h_samp_factor) - 1;

		component->width_in_sblocks[sampling] = component->width_in_blocks;
		component->height_in_sblocks[sampling] = component->height_in_blocks;

		component->sblock[sampling] = component->block;

		component->width_in_blocks = 0;
		component->height_in_blocks = 0;
		component->block = NULL;

		if(sampling == 0)
			modjpeg_downsample_component(mj, component);
		else
			modjpeg_upsample_component(mj, component);
	}

	return 0;
}

/* Downsample von einer Komponente berechnen */
int modjpeg_downsample_component(modjpeg *mj, modjpeg_image_component *component) {
	int i, j;
	float t1[MODJPEG_BLOCKSIZE2], t2[MODJPEG_BLOCKSIZE2], t3[MODJPEG_BLOCKSIZE2], t4[MODJPEG_BLOCKSIZE2];
	float x[MODJPEG_BLOCKSIZE2], y[MODJPEG_BLOCKSIZE2];
	modjpeg_image_block **b;

	memset(t1, 0, MODJPEG_BLOCKSIZE2 * sizeof(float));
	memset(t2, 0, MODJPEG_BLOCKSIZE2 * sizeof(float));
	memset(t3, 0, MODJPEG_BLOCKSIZE2 * sizeof(float));
	memset(t4, 0, MODJPEG_BLOCKSIZE2 * sizeof(float));
	memset(x, 0, MODJPEG_BLOCKSIZE2 * sizeof(float));
	memset(y, 0, MODJPEG_BLOCKSIZE2 * sizeof(float));

	/* Neue Hoehe und Breite berechnen */
	component->width_in_blocks = (component->width_in_sblocks[0] >> 1);
	component->height_in_blocks = (component->height_in_sblocks[0] >> 1);

	component->block = modjpeg_alloc_component_block(mj, component->width_in_blocks, component->height_in_blocks);
	if(component->block == NULL)
		return -1;

	/* Original Bloecke */
	b = component->sblock[0];

	/* Immer 4 Bloecke zu einem neuen Block schrumpfen */
	for(i = 0; i < component->height_in_sblocks[0]; i += 2) {
		for(j = 0; j < component->width_in_sblocks[0]; j += 2) {
			// t1 = b1 + b3
			modjpeg_matrix_add(t1, b[i][j], b[i + 1][j], 4, 4);
			// t2 = b1 - b3
			modjpeg_matrix_sub(t2, b[i][j], b[i + 1][j], 4, 4);

			// t3 = c * t1
			modjpeg_matrix_mul(t3, mj->resamp_c, 8, 4, t1, 4, 4);
			// t4 = d * t2
			modjpeg_matrix_mul(t4, mj->resamp_d, 8, 4, t2, 4, 4);

			// x = t3 + t4
			modjpeg_matrix_add(x, t3, t4, 8, 4);


			// t1 = b2 + b4
			modjpeg_matrix_add(t1, b[i][j + 1], b[i + 1][j + 1], 4, 4);
			// t2 = b2 - b4
			modjpeg_matrix_sub(t2, b[i][j + 1], b[i + 1][j + 1], 4, 4);

			// t3 = c * t1
			modjpeg_matrix_mul(t3, mj->resamp_c, 8, 4, t1, 4, 4);
			// t4 = d * t2
			modjpeg_matrix_mul(t4, mj->resamp_d, 8, 4, t2, 4, 4);

			// y = t3 + t4
			modjpeg_matrix_add(y, t3, t4, 8, 4);

			// t1 = x + y
			modjpeg_matrix_add(t1, x, y, 8, 4);
			// t2 = x - y
			modjpeg_matrix_sub(t2, x, y, 8, 4);

			// t3 = t1 * ct
			modjpeg_matrix_mul(t3, t1, 8, 4, mj->resamp_ct, 4, 8);
			// t4 = t2 * dt
			modjpeg_matrix_mul(t4, t2, 8, 4, mj->resamp_dt, 4, 8);

			// component->block = t3 + t4
			modjpeg_matrix_add(component->block[(i >> 1)][(j >> 1)], t3, t4, 8, 8);
		}
	}

	/* Das neue Bild in das Array mit den Samplings verschieben */
	component->width_in_sblocks[1] = component->width_in_blocks;
	component->height_in_sblocks[1] = component->height_in_blocks;

	component->sblock[1] = component->block;

	component->width_in_blocks = 0;
	component->height_in_blocks = 0;
	component->block = NULL;

	return 0;
}

int modjpeg_upsample_component(modjpeg *mj, modjpeg_image_component *component) {
	int i, j;
	float t1[MODJPEG_BLOCKSIZE2], t2[MODJPEG_BLOCKSIZE2], t3[MODJPEG_BLOCKSIZE2], t4[MODJPEG_BLOCKSIZE2];
	float p[MODJPEG_BLOCKSIZE2], q[MODJPEG_BLOCKSIZE2], r[MODJPEG_BLOCKSIZE2], s[MODJPEG_BLOCKSIZE2];
	modjpeg_image_block b;

	memset(t1, 0, MODJPEG_BLOCKSIZE2 * sizeof(float));
	memset(t2, 0, MODJPEG_BLOCKSIZE2 * sizeof(float));
	memset(t3, 0, MODJPEG_BLOCKSIZE2 * sizeof(float));
	memset(t4, 0, MODJPEG_BLOCKSIZE2 * sizeof(float));
	memset(p, 0, MODJPEG_BLOCKSIZE2 * sizeof(float));
	memset(q, 0, MODJPEG_BLOCKSIZE2 * sizeof(float));
	memset(r, 0, MODJPEG_BLOCKSIZE2 * sizeof(float));
	memset(s, 0, MODJPEG_BLOCKSIZE2 * sizeof(float));

	/* Neue Hoehe und Breite berechnen */
	component->width_in_blocks = (component->width_in_sblocks[1] << 1);
	component->height_in_blocks = (component->height_in_sblocks[1] << 1);

	component->block = modjpeg_alloc_component_block(mj, component->width_in_blocks, component->height_in_blocks);
	if(component->block == NULL)
		return -1;

	/* Immer 1 Block zu 4 neuen Bloecken erweitern */
	for(i = 0; i < component->height_in_sblocks[1]; i++) {
		for(j = 0; j < component->width_in_sblocks[1]; j++) {
			// Original Block
			b = component->sblock[1][i][j];

			// t1 = b * e
			modjpeg_matrix_mul(t1, b, 8, 8, mj->resamp_e, 8, 4);
			// p = et * t1
			modjpeg_matrix_mul(p, mj->resamp_et, 4, 8, t1, 8, 4);

			// t1 = b * f
			modjpeg_matrix_mul(t1, b, 8, 8, mj->resamp_f, 8, 4);
			// q = et * t1
			modjpeg_matrix_mul(q, mj->resamp_et, 4, 8, t1, 8, 4);

			// t1 = b * e
			modjpeg_matrix_mul(t1, b, 8, 8, mj->resamp_e, 8, 4);
			// r = ft * t1
			modjpeg_matrix_mul(r, mj->resamp_ft, 4, 8, t1, 8, 4);

			// t1 = b * f
			modjpeg_matrix_mul(t1, b, 8, 8, mj->resamp_f, 8, 4);
			// s = ft * t1
			modjpeg_matrix_mul(s, mj->resamp_ft, 4, 8, t1, 8, 4);

			// t1 = p + q
			modjpeg_matrix_add(t1, p, q, 4, 4);
			// t2 = p - q
			modjpeg_matrix_sub(t2, p, q, 4, 4);
			// t3 = r + s
			modjpeg_matrix_add(t3, r, s, 4, 4);
			// t4 = r - s
			modjpeg_matrix_sub(t4, r, s, 4, 4);

			// b1 = t1 + t3
			modjpeg_matrix_add(component->block[(i << 1)][(j << 1)], t1, t3, 4, 4);
			// b2 = t2 + t4
			modjpeg_matrix_add(component->block[(i << 1)][(j << 1) + 1], t2, t4, 4, 4);
			// b3 = t1 - t3
			modjpeg_matrix_sub(component->block[(i << 1) + 1][(j << 1)], t1, t3, 4, 4);
			// b4 = t2 - t4
			modjpeg_matrix_sub(component->block[(i << 1) + 1][(j << 1) + 1], t2, t4, 4, 4);
		}
	}

	/* Das neue Bild in das Array mit den Samplings verschieben */
	component->width_in_sblocks[0] = component->width_in_blocks;
	component->height_in_sblocks[0] = component->height_in_blocks;

	component->sblock[0] = component->block;

	component->width_in_blocks = 0;
	component->height_in_blocks = 0;
	component->block = NULL;

	return 0;
}
