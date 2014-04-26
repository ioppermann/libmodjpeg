/*
 * init.c
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
#include "init.h"
#include "dct.h"
#include "resample.h"
#include "image.h"
#include "logo.h"
#include "watermark.h"

int modjpeg_init(modjpeg *mj) {
	if(mj == NULL)
		return -1;

	memset(mj, 0, sizeof(modjpeg));

	modjpeg_resample_init(mj);
	modjpeg_costable_init(mj);

	return 0;
}

int modjpeg_reset(modjpeg *mj) {
	if(mj == NULL)
		return -1;

	modjpeg_unset_logo_all(mj);
	if(mj->logos != NULL)
		free(mj->logos);
	mj->logos = NULL;
	if(mj->masks != NULL)
		free(mj->masks);
	mj->masks = NULL;

	mj->clogos = 0;

	modjpeg_unset_watermark_all(mj);
	if(mj->watermarks != NULL)
		free(mj->watermarks);
	mj->watermarks = NULL;

	mj->cwatermarks = 0;

	return 0;
}

int modjpeg_destroy(modjpeg *mj) {
	if(mj == NULL)
		return -1;

	modjpeg_reset(mj);

	return 0;
}
