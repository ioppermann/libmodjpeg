/*
 * memory.c
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
#include "memory.h"

modjpeg_image_block **modjpeg_alloc_component_block(modjpeg *mj, int width, int height) {
	int i, j;
	modjpeg_image_block **block;

	block = (modjpeg_image_block **)calloc(height, sizeof(modjpeg_image_block *));
	if(block == NULL)
		return NULL;

	for(i = 0; i < height; i++) {
		block[i] = (modjpeg_image_block *)calloc(width, sizeof(modjpeg_image_block));
		if(block[i] == NULL) {
			modjpeg_free_component_block(mj, block, width, height);
			return NULL;
		}

		for(j = 0; j < width; j++) {
			block[i][j] = (modjpeg_image_block)calloc(MODJPEG_BLOCKSIZE2, sizeof(float));
			if(block[i][j] == NULL) {
				modjpeg_free_component_block(mj, block, width, height);
				return NULL;
			}
		}
	}

	return block;
}

modjpeg_image *modjpeg_alloc_image(modjpeg *mj, int components) {
	int i;
	modjpeg_image *image;

	image = (modjpeg_image *)calloc(1, sizeof(modjpeg_image));
	if(image == NULL)
		return NULL;

	image->num_components = components;

	image->component = (modjpeg_image_component **)calloc(image->num_components, sizeof(modjpeg_image_component *));
	if(image->component == NULL) {
		modjpeg_free_image(mj, image);
		return NULL;
	}

	for(i = 0; i < image->num_components; i++) {
		image->component[i] = (modjpeg_image_component *)calloc(1, sizeof(modjpeg_image_component));
		if(image->component[i] == NULL) {
			modjpeg_free_image(mj, image);
			return NULL;
		}
	}

	return image;
}

void modjpeg_free_component_block(modjpeg *mj, modjpeg_image_block **block, int width, int height) {
	int i, j;

	if(block == NULL)
		return;

	for(i = 0; i < height; i++) {
		if(block[i] == NULL)
			continue;

		for(j = 0; j < width; j++) {
			if(block[i][j] == NULL)
				continue;

			free(block[i][j]);
		}

		free(block[i]);
	}

	free(block);

	return;
}

void modjpeg_free_component(modjpeg *mj, modjpeg_image_component *component) {
	int i;

	if(component == NULL)
		return;

	for(i = 0; i < 2; i++)
		modjpeg_free_component_block(mj, component->sblock[i], component->width_in_sblocks[i], component->height_in_sblocks[i]);

	free(component);

	return;
}

void modjpeg_free_image(modjpeg *mj, modjpeg_image *image) {
	int i;

	if(image == NULL)
		return;

	for(i = 0; i < image->num_components; i++)
		modjpeg_free_component(mj, image->component[i]);

	free(image);

	return;
}
