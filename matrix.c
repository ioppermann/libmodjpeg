/*
 * matrix.c
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
#include "matrix.h"

/* Multipliziert zwei Matrizen. Achtung: Basiert auf 8x8 Matrizen! */
void modjpeg_matrix_mul(float *res, float *a, int ah, int aw, float *b, int bh, int bw) {
	int i, j, k;

	// aw == bh !!

	for(i = 0; i < ah; i++) {
		for(j = 0; j < bw; j++) {
			res[(i << 3) + j] = 0.0;
			for(k = 0; k < bh; k++)
				res[(i << 3) + j] += a[(i << 3) + k] * b[(k << 3) + j];
		}
	}

	return;
}

/* Addiert zwei Matrizen. Achtung: Basiert auf 8x8 Matrizen! */
void modjpeg_matrix_add(float *res, float *a, float *b, int h, int w) {
	int i, j, k;

	for(i = 0; i < h; i++) {
		for(j = 0; j < w; j++) {
			k = (i << 3) + j;
			res[k] = a[k] + b[k];
		}
	}

	return;
}

/* Subtrahiert zwei Matrizen. Achtung: Basiert auf 8x8 Matrizen! */
void modjpeg_matrix_sub(float *res, float *a, float *b, int h, int w) {
	int i, j, k;

	for(i = 0; i < h; i++) {
		for(j = 0; j < w; j++) {
			k = (i << 3) + j;
			res[k] = a[k] - b[k];
		}
	}

	return;
}
