/* rough outline

typedef struct {
	width_pixel, height_pixel
	width_blocks, height_blocks

	h_sampling, v_sampling
	colorspace

	cinfo
	coef
} jpegimage;

typedef struct {
	colorspace
	rawdata
	width_pixel, height_pixel

	jpegimage rgb
	jpegimage alpha
} dropon;

jpegimage *create_image(buffer, len)
dropon    *create_dropon(buffer, (RGB|RGBA|A|GRAY|GRAYA), width, height)

compose(jpegimage, dropon, (TOP|BOTTOM|CENTER), (LEFT|CENTER|RIGHT), offset_x, offset_y)
pixelate(jpegimage, area)
tint(jpegimage, ...)
grayscale(jpegimage)
luminance(jpegimage, ..)

store(jpegimage, *buffer, *len)

destroy_image(jpegimage)
destroy_dropon(dropon)

jpegimage *create_image(buffer, len) {
	read jpeg from buffer

	store values in struct
}

dropon *create_dropon(buffer, (RGB|RGBA|A|GRAY|GRAYA), width, height) {
	normalize to RGBA
	store values in struct

	color = null
	alpa = null
}

compose(jpegimage, dropon, align_h, align_v, offset_x, offset_y) {
	is the colorspace the same as jpegimage?
	is the sampling the same as jpegimage?

	any no:
		- destroy_image(color)
		- destroy_image(alpha)
		- encode from rawdata with same colorspace and sampling, use highest quality
		- color = create_image(encoded)
		- alpha = create_image(encoded alpha component)

	compose_and_mask(jpegimage, color, alpha, align_h, align_v)
}

compose_and_mask(jpegimage, color, alpha, align_h, align_v) {
	find affected blocks in jpegimage
		- use horizontal alignment
		- use vertival alignment

	iterate over affected blocks in all components of jpegimage
		- apply color (and mask if available)
}

pixelate(jpegimage, area) {
	pixelate the given area
}

tint(jpegimage, ...) {
	modify Cr and or Cb
}

grayscale(jpegimage) {
	remove Cr and Cb
}

luminance(jpegimage, ..) {
	increase Y component
}

store(jpegimage, *buffer, *len) {
	write image into buffer
}

destroy_image(jpegimage) {
	free jpegimage struct
}

destroy_dropon(dropon) {
	free dropon struct
}
*/

// gcc outline.c -o outline -Wall -O2 -lm -ljpeg -I/opt/local/include -L/opt/local/lib

#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>

#include "jpeglib.h"
#include "jerror.h"

#define MODJPEG_DESTBUFFER_CHUNKSIZE	2048

typedef modjpeg_error_mgr {
	struct jpeg_error_mgr pub;

	jmp_buf setjmp_buffer;
};

struct modjpeg_dest_mgr {
	struct jpeg_destination_mgr pub;

	JOCTET *buf;
	size_t size;
};

struct modjpeg_src_mgr {
	struct jpeg_source_mgr pub;

	JOCTET *buf;
	size_t size;
};

typedef struct modjpeg_error_mgr* modjpeg_error_ptr;
typedef struct modjpeg_src_mgr *modjpeg_src_ptr;
typedef struct modjpeg_dest_mgr* modjpeg_dest_ptr;

void modjpeg_init_source(j_decompress_ptr cinfo);
boolean modjpeg_fill_input_buffer(j_decompress_ptr cinfo);
void modjpeg_skip_input_data(j_decompress_ptr cinfo, long num_bytes);
void modjpeg_term_source(j_decompress_ptr cinfo);

void modjpeg_error_exit(j_common_ptr cinfo);
void modjpeg_init_destination(j_compress_ptr cinfo);
boolean modjpeg_empty_output_buffer(j_compress_ptr cinfo);
void modjpeg_term_destination(j_compress_ptr cinfo);

typedef struct {
	struct jpeg_decompress_struct cinfo;
	jvirt_barray_ptr *coef;
} modjpeg_jpegimage_t;

typedef struct {
	int colorspace;
	char *rawdata;
	size_t rawdata_len;

	int width_pixel;
	int height_pixel;

	modjpeg_jpegimage_t color;
	modjpeg_jpegimage_t alpha;
} modjpeg_dropon_t;

modjpeg_jpegimage_t *create_image(const char *buffer, size_t len);
int store_image(modjpeg_jpegimage_t *m, char **buffer, size_t *len);

int main(int argc, char **argv) {
	return 0;
}

modjpeg_jpegimage_t *create_image(const char *buffer, size_t len) {
	modjpeg_jpegimage_t *m;
	struct modjpeg_error_mgr jerr;
	struct modjpeg_src_mgr src;
	jpeg_component_info *component;

	if(buffer == NULL || len == 0) {
		return NULL;
	}

	m = (modjpeg_jpegimage_t *)calloc(1, sizeof(modjpeg_jpegimage_t));
	if(m == NULL) {
		return NULL;
	}

	m->cinfo.err = jpeg_std_error(&jerr.pub);
	jerr.pub.error_exit = modjpeg_error_exit;
	if(setjmp(jerr.setjmp_buffer)) {
		jpeg_destroy_decompress(&m->cinfo);
		if(fp != NULL)
			fclose(fp);

		free(m);

		return NULL;
	}

	jpeg_create_decompress(&m->cinfo);

	m->cinfo.src = &src.pub;
	src.pub.init_source = modjpeg_init_source;
	src.pub.fill_input_buffer = modjpeg_fill_input_buffer;
	src.pub.skip_input_data = modjpeg_skip_input_data;
	src.pub.resync_to_restart = jpeg_resync_to_restart;
	src.pub.term_source = modjpeg_term_source;

	src.buf = (JOCTET *)image;
	src.size = (size_t)image_size;

	jpeg_read_header(&m->cinfo, TRUE);

	m->coef = jpeg_read_coefficients(&m->cinfo);

	return m;
}

int store_image(modjpeg_jpegimage_t *m, char **buffer, size_t *len) {
	struct jpeg_compress_struct cinfo;
	jvirt_barray_ptr *dst_coef_arrays;
	struct modjpeg_error_mgr jerr;
	struct modjpeg_dest_mgr dest;
	char jpegerrorbuffer[JMSG_LENGTH_MAX];

	cinfo.err = jpeg_std_error(&jerr.pub);
	jerr.pub.error_exit = modjpeg_error_exit;
	if(setjmp(jerr.setjmp_buffer)) {
		(*cinfo.err->format_message)((j_common_ptr)&cinfo, jpegerrorbuffer);
		jpeg_destroy_compress(&cinfo);
		if(dest.buf != NULL) {
			free(dest.buf);
		}

		return -1;
	}

	jpeg_create_compress(&cinfo);

	cinfo.dest = &dest.pub;
	dest.buf = NULL;
	dest.size = 0;
	dest.pub.init_destination = modjpeg_init_destination;
	dest.pub.empty_output_buffer = modjpeg_empty_output_buffer;
	dest.pub.term_destination = modjpeg_term_destination;

	jpeg_copy_critical_parameters(&m->cinfo, &cinfo);

	cinfo.optimize_coding = FALSE;
	//if((options & MODJPEG_OPTIMIZE) != 0)
	//	cinfo.optimize_coding = TRUE;

	dst_coef_arrays = m->coef;

	/* Die neuen Koeffizienten speichern */
	jpeg_write_coefficients(&cinfo, dst_coef_arrays);

	jpeg_finish_compress(&cinfo);
	jpeg_destroy_compress(&cinfo);

	*buffer = (char *)dest.buf;
	*len = dest.size;

	return 0;
}


/** JPEG reading and writing **/

void modjpeg_error_exit(j_common_ptr cinfo) {
	modjpeg_error_ptr myerr = (modjpeg_error_ptr)cinfo->err;

	longjmp(myerr->setjmp_buffer, 1);
}

void modjpeg_init_destination(j_compress_ptr cinfo) {
	modjpeg_dest_ptr dest = (modjpeg_dest_ptr)cinfo->dest;

	dest->buf = (JOCTET *)malloc(MODJPEG_DESTBUFFER_CHUNKSIZE * sizeof(JOCTET));
	if(dest->buf == NULL)
		ERREXIT1(cinfo, JERR_OUT_OF_MEMORY, 0);
	dest->size = MODJPEG_DESTBUFFER_CHUNKSIZE;

	dest->pub.next_output_byte = dest->buf;
	dest->pub.free_in_buffer = MODJPEG_DESTBUFFER_CHUNKSIZE;

	return;
}

boolean modjpeg_empty_output_buffer(j_compress_ptr cinfo) {
	JOCTET *ret;
	modjpeg_dest_ptr dest = (modjpeg_dest_ptr)cinfo->dest;

	ret = (JOCTET *)realloc(dest->buf, (dest->size + MODJPEG_DESTBUFFER_CHUNKSIZE) * sizeof(JOCTET));
	if(ret == NULL)
		ERREXIT1(cinfo, JERR_OUT_OF_MEMORY, 0);
	dest->buf = ret;
	dest->size += MODJPEG_DESTBUFFER_CHUNKSIZE;

	dest->pub.next_output_byte = dest->buf + (dest->size - MODJPEG_DESTBUFFER_CHUNKSIZE);
	dest->pub.free_in_buffer = MODJPEG_DESTBUFFER_CHUNKSIZE;

	return TRUE;
}

void modjpeg_term_destination(j_compress_ptr cinfo) {
	modjpeg_dest_ptr dest = (modjpeg_dest_ptr)cinfo->dest;

	dest->size -= dest->pub.free_in_buffer;

	return;
}

void modjpeg_init_source(j_decompress_ptr cinfo) {
	modjpeg_src_ptr src = (modjpeg_src_ptr)cinfo->src;

	src->pub.bytes_in_buffer = 0;
	src->pub.next_input_byte = NULL;

	return;
}

boolean modjpeg_fill_input_buffer(j_decompress_ptr cinfo) {
	modjpeg_src_ptr src = (modjpeg_src_ptr)cinfo->src;

	src->pub.next_input_byte = src->buf;
	src->pub.bytes_in_buffer = src->size;

	return TRUE;
}

void modjpeg_skip_input_data(j_decompress_ptr cinfo, long num_bytes) {
	modjpeg_src_ptr src = (modjpeg_src_ptr)cinfo->src;

	src->pub.next_input_byte += (size_t)num_bytes;
	src->pub.bytes_in_buffer -= (size_t)num_bytes;
}

void modjpeg_term_source(j_decompress_ptr cinfo) {
  /* no work necessary here */
}