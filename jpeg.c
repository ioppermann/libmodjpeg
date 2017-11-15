/*
 * Copyright (c) 2006-2017 Ingo Oppermann
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

#include <stdlib.h>
#include <setjmp.h>

#include "libmodjpeg.h"
#include "jpeg.h"
#include "jpeglib.h"
#include "jerror.h"

/** JPEG reading and writing **/

void mj_jpeg_error_exit(j_common_ptr cinfo) {
	mj_jpeg_error_ptr myerr = (mj_jpeg_error_ptr)cinfo->err;

	longjmp(myerr->setjmp_buffer, 1);
}

void mj_jpeg_init_destination(j_compress_ptr cinfo) {
	mj_jpeg_dest_ptr dest = (mj_jpeg_dest_ptr)cinfo->dest;

	dest->buf = (JOCTET *)malloc(MJ_DESTBUFFER_CHUNKSIZE * sizeof(JOCTET));
	if(dest->buf == NULL) {
		ERREXIT1(cinfo, JERR_OUT_OF_MEMORY, 0);
	}
	dest->size = MJ_DESTBUFFER_CHUNKSIZE;

	dest->pub.next_output_byte = dest->buf;
	dest->pub.free_in_buffer = MJ_DESTBUFFER_CHUNKSIZE;

	return;
}

boolean mj_jpeg_empty_output_buffer(j_compress_ptr cinfo) {
	JOCTET *ret;
	mj_jpeg_dest_ptr dest = (mj_jpeg_dest_ptr)cinfo->dest;

	ret = (JOCTET *)realloc(dest->buf, (dest->size + MJ_DESTBUFFER_CHUNKSIZE) * sizeof(JOCTET));
	if(ret == NULL) {
		ERREXIT1(cinfo, JERR_OUT_OF_MEMORY, 0);
	}
	dest->buf = ret;
	dest->size += MJ_DESTBUFFER_CHUNKSIZE;

	dest->pub.next_output_byte = dest->buf + (dest->size - MJ_DESTBUFFER_CHUNKSIZE);
	dest->pub.free_in_buffer = MJ_DESTBUFFER_CHUNKSIZE;

	return TRUE;
}

void mj_jpeg_term_destination(j_compress_ptr cinfo) {
	mj_jpeg_dest_ptr dest = (mj_jpeg_dest_ptr)cinfo->dest;

	dest->size -= dest->pub.free_in_buffer;

	return;
}

void mj_jpeg_init_source(j_decompress_ptr cinfo) {
	mj_jpeg_src_ptr src = (mj_jpeg_src_ptr)cinfo->src;

	src->pub.bytes_in_buffer = 0;
	src->pub.next_input_byte = NULL;

	return;
}

boolean mj_jpeg_fill_input_buffer(j_decompress_ptr cinfo) {
	mj_jpeg_src_ptr src = (mj_jpeg_src_ptr)cinfo->src;

	src->pub.next_input_byte = src->buf;
	src->pub.bytes_in_buffer = src->size;

	return TRUE;
}

void mj_jpeg_skip_input_data(j_decompress_ptr cinfo, long num_bytes) {
	mj_jpeg_src_ptr src = (mj_jpeg_src_ptr)cinfo->src;

	src->pub.next_input_byte += (size_t)num_bytes;
	src->pub.bytes_in_buffer -= (size_t)num_bytes;
}

void mj_jpeg_term_source(j_decompress_ptr cinfo) {
	/* no work necessary here */
}


