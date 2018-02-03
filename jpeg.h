/*
 * Copyright (c) 2006+ Ingo Oppermann
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
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
