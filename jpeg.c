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
	if(dest->buf == NULL)
		ERREXIT1(cinfo, JERR_OUT_OF_MEMORY, 0);
	dest->size = MJ_DESTBUFFER_CHUNKSIZE;

	dest->pub.next_output_byte = dest->buf;
	dest->pub.free_in_buffer = MJ_DESTBUFFER_CHUNKSIZE;

	return;
}

boolean mj_jpeg_empty_output_buffer(j_compress_ptr cinfo) {
	JOCTET *ret;
	mj_jpeg_dest_ptr dest = (mj_jpeg_dest_ptr)cinfo->dest;

	ret = (JOCTET *)realloc(dest->buf, (dest->size + MJ_DESTBUFFER_CHUNKSIZE) * sizeof(JOCTET));
	if(ret == NULL)
		ERREXIT1(cinfo, JERR_OUT_OF_MEMORY, 0);
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


