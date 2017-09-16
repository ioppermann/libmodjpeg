/*
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

#ifndef _LIBMODJPEG_JPEG_H_
#define _LIBMODJPEG_JPEG_H_

#include <setjmp.h>

#include "libmodjpeg.h"
#include "jpeglib.h"

#define MJ_DESTBUFFER_CHUNKSIZE		2048

struct mj_jpeg_error_mgr {
	struct jpeg_error_mgr pub;

	jmp_buf setjmp_buffer;
};

struct mj_jpeg_dest_mgr {
	struct jpeg_destination_mgr pub;

	JOCTET *buf;
	size_t size;
};

struct mj_jpeg_src_mgr {
	struct jpeg_source_mgr pub;

	JOCTET *buf;
	size_t size;
};

typedef struct mj_jpeg_error_mgr* mj_jpeg_error_ptr;
typedef struct mj_jpeg_src_mgr *mj_jpeg_src_ptr;
typedef struct mj_jpeg_dest_mgr* mj_jpeg_dest_ptr;

void mj_jpeg_init_source(j_decompress_ptr cinfo);
boolean mj_jpeg_fill_input_buffer(j_decompress_ptr cinfo);
void mj_jpeg_skip_input_data(j_decompress_ptr cinfo, long num_bytes);
void mj_jpeg_term_source(j_decompress_ptr cinfo);

void mj_jpeg_error_exit(j_common_ptr cinfo);
void mj_jpeg_init_destination(j_compress_ptr cinfo);
boolean mj_jpeg_empty_output_buffer(j_compress_ptr cinfo);
void mj_jpeg_term_destination(j_compress_ptr cinfo);

#endif
