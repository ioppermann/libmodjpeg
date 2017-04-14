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
#include <string.h>
#include <setjmp.h>
#include <sys/stat.h>

#include "jpeglib.h"
#include "jerror.h"

#define MODJPEG_DESTBUFFER_CHUNKSIZE	2048

#define MODJPEG_COLORSPACE_RGBA		1
#define MODJPEG_COLORSPACE_RGB		2
#define MODJPEG_COLORSPACE_GRAYSCALE	3
#define MODJPEG_COLORSPACE_GRAYSCALEA	4

#define MODJPEG_ALIGN_TOP		1
#define MODJPEG_ALIGN_BOTTOM		2
#define MODJPEG_ALIGN_LEFT		3
#define MODJPEG_ALIGN_RIGHT		4
#define MODJPEG_ALIGN_CENTER		5

struct modjpeg_error_mgr {
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
	unsigned int colorspace;
	char *rawimage;
	char *rawalpha;

	size_t width;
	size_t height;

	modjpeg_jpegimage_t *image;
	modjpeg_jpegimage_t *alpha;
} modjpeg_dropon_t;

modjpeg_jpegimage_t *create_image(const char *buffer, size_t len);
modjpeg_dropon_t *create_dropon(const char *buffer, unsigned int colorspace, size_t width, size_t height);

int store_image(modjpeg_jpegimage_t *m, char **buffer, size_t *len);

modjpeg_jpegimage_t *read_image(char *filename);
int write_image(modjpeg_jpegimage_t *m, char *filename);

void destroy_image(modjpeg_jpegimage_t *m);

int compose(modjpeg_jpegimage_t *m, modjpeg_dropon_t *d, int align_h, int align_v, int offset_x, int offset_y);

int encode_jpeg(char **buffer, size_t *len, unsigned char *rawdata, int colorspace, int width, int height);
int decode_jpeg(char **buffer, size_t *len, int *colorspace, int *width, int *height, const char *filename);

int main(int argc, char **argv) {
	modjpeg_jpegimage_t *m;
	modjpeg_dropon_t *d;

	m = read_image("./images/in.jpg");
	write_image(m, "./images/out.jpg");

	char *buffer = NULL;
	int colorspace = 0;
	int width = 0;
	int height = 0;
	size_t len = 0;

	if(decode_jpeg(&buffer, &len, &colorspace, &width, &height, "./images/logo.jpg") == -1) {
		printf("can't decode jpeg\n");
		exit(1);
	}

	printf("%dx%d pixel, %ld bytes\n", width, height, len);

	d = create_dropon(buffer, colorspace, width, height);

	compose(m, d, 0, 0, 0, 0);

	write_image(d->image, "./images/test.jpg");

	return 0;
}

int compose(modjpeg_jpegimage_t *m, modjpeg_dropon_t *d, int align_h, int align_v, int offset_x, int offset_y) {
	int reload_dropon = 0;
	jpeg_component_info *component_m = NULL, *component_d = NULL;

	if(d->image != NULL) {
		// is the colorspace the same as jpegimage?
		if(m->cinfo.jpeg_color_space != d->image->cinfo.jpeg_color_space) {
			reload_dropon = 1;
		}

		// is the sampling the same as jpegimage?
		int c = 0;
		for(c = 0; c < m->cinfo.num_components; c++) {
			component_m = &m->cinfo.comp_info[c];
			component_d = &d->image->cinfo.comp_info[c];

			if(component_m->h_samp_factor != component_d->h_samp_factor) {
				reload_dropon = 1;
			}

			if(component_m->v_samp_factor != component_d->v_samp_factor) {
				reload_dropon = 1;
			}
		}
	}
	else {
		reload_dropon = 1;
	}

/*
	any no:
		- destroy_image(color)
		- destroy_image(alpha)
		- encode from rawdata with same colorspace and sampling, use highest quality
		- color = create_image(encoded)
		- alpha = create_image(encoded alpha component)
*/

	if(reload_dropon == 1) {
		printf("reloading dropon\n");

		destroy_image(d->image);
		destroy_image(d->alpha);

		char *buffer = NULL;
		size_t len = 0;

		encode_jpeg(&buffer, &len, (unsigned char *)d->rawimage, MODJPEG_COLORSPACE_RGB, d->width, d->height);
		printf("encoded len: %ld\n", len);
		d->image = create_image(buffer, len);
		free(buffer);

		if(d->rawalpha != NULL) {
			encode_jpeg(&buffer, &len, (unsigned char *)d->rawalpha, MODJPEG_COLORSPACE_GRAYSCALE, d->width, d->height);
			d->alpha = create_image(buffer, len);
			free(buffer);
		}
	}

	//compose_and_mask(m, d->color, d->alpha, align_h, align_v);

	return 0;
}

int encode_jpeg(char **buffer, size_t *len, unsigned char *rawdata, int colorspace, int width, int height) {
	struct jpeg_compress_struct cinfo;
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

	cinfo.image_width = width;
	cinfo.image_height = height;
	cinfo.input_components = 3;
	cinfo.in_color_space = JCS_RGB;

	jpeg_set_defaults(&cinfo);

	cinfo.optimize_coding = TRUE;

	if(colorspace == MODJPEG_COLORSPACE_RGB) {
		jpeg_set_colorspace(&cinfo, JCS_RGB);
	}
	else if(colorspace == MODJPEG_COLORSPACE_GRAYSCALE) {
		jpeg_set_colorspace(&cinfo, JCS_GRAYSCALE);
	}

	jpeg_set_quality(&cinfo, 100, TRUE);

	jpeg_start_compress(&cinfo, TRUE);

	int row_stride = cinfo.image_width * cinfo.input_components;

	JSAMPROW row_pointer[1];

	while(cinfo.next_scanline < cinfo.image_height) {
		printf("%d ", cinfo.next_scanline * row_stride);
		row_pointer[0] = &rawdata[cinfo.next_scanline * row_stride];
		jpeg_write_scanlines(&cinfo, row_pointer, 1);
	}

	jpeg_finish_compress(&cinfo);
	jpeg_destroy_compress(&cinfo);

	*buffer = (char *)dest.buf;
	*len = dest.size;

	return 0;
}

void destroy_image(modjpeg_jpegimage_t *m) {
	if(m == NULL) {
		return;
	}

	jpeg_destroy_decompress(&m->cinfo);

	m->coef = NULL;

	free(m);

	return;
}

modjpeg_dropon_t *create_dropon(const char *buffer, unsigned int colorspace, size_t width, size_t height) {
	modjpeg_dropon_t *d;
	int ncomponents = 0;

	if(buffer == NULL) {
		return NULL;
	}

	switch(colorspace) {
/*
		case MODJPEG_COLORSPACE_GRAYSCALE:
			ncomponents = 1;
			d->colorspace = MODJPEG_COLORSPACE_GRAYSCALE;
			break;
		case MODJPEG_COLORSPACE_GRAYSCALEA:
			ncomponents = 2;
			d->colorspace = MODJPEG_COLORSPACE_GRAYSCALE;
			break;
*/
		case MODJPEG_COLORSPACE_RGB:
			ncomponents = 3;
			break;
		case MODJPEG_COLORSPACE_RGBA:
			ncomponents = 4;
			break;
		default:
			printf("unsupported colorspace");
			return NULL;
	}

	d = (modjpeg_dropon_t *)calloc(1, sizeof(modjpeg_dropon_t));
	if(d == NULL) {
		printf("can't allocate dropon");
		return NULL;
	}

	d->colorspace = MODJPEG_COLORSPACE_RGB;
	d->width = width;
	d->height = height;

	// image
	size_t nsamples = 3 * width * height;

	d->rawimage = (char *)calloc(nsamples, sizeof(char));
	if(d->rawimage == NULL) {
		printf("can't allocate buffer");
		return NULL;
	}

	if(colorspace == MODJPEG_COLORSPACE_RGBA) {	
		nsamples = 1 * width * height;

		d->rawalpha = (char *)calloc(nsamples, sizeof(char));
		if(d->rawalpha == NULL) {
			printf("can't allocate buffer");
			return NULL;
		}

		const char *p = buffer;
		char *pimage = d->rawimage;
		char *palpha = d->rawalpha;

		size_t v = 0;
		for(v = 0; v < (width * height); v++) {
			*pimage++ = *p++;
			*pimage++ = *p++;
			*pimage++ = *p++;
			*palpha++ = *p++;
		}
	}
	else {
		const char *p = buffer;
		char *pimage = d->rawimage;

		size_t v = 0;
		for(v = 0; v < (width * height); v++) {
			*pimage++ = *p++;
			*pimage++ = *p++;
			*pimage++ = *p++;
		}
	}

	return d;
}

modjpeg_jpegimage_t *read_image(char *filename) {
	FILE *fp;
	struct stat s;
	char *buffer;
	size_t len;

	fp = fopen(filename, "rb");
	if(fp == NULL) {
		printf("can't open input file\n");
		return NULL;
	}

	fstat(fileno(fp), &s);

	len = (size_t)s.st_size;

	buffer = (char *)calloc(len, sizeof(char));
	if(buffer == NULL) {
		printf("can't allocate memory for filedata\n");
		return NULL;
	}

	fread(buffer, 1, len, fp);

	fclose(fp);

	modjpeg_jpegimage_t *m;

	m = create_image(buffer, len);
	free(buffer);
	if(m == NULL) {
		printf("reading from buffer failed\n");
		return NULL;
	}

	return m;
}

int decode_jpeg(char **buffer, size_t *len, int *colorspace, int *width, int *height, const char *filename) {
	FILE *fp;
	struct jpeg_decompress_struct cinfo;
	struct modjpeg_error_mgr jerr;
	char jpegerrorbuffer[JMSG_LENGTH_MAX];

	cinfo.err = jpeg_std_error(&jerr.pub);
	jerr.pub.error_exit = modjpeg_error_exit;
	if(setjmp(jerr.setjmp_buffer)) {
		(*cinfo.err->format_message)((j_common_ptr)&cinfo, jpegerrorbuffer);
		jpeg_destroy_decompress(&cinfo);
		fclose(fp);
		return -1;
	}

	jpeg_create_decompress(&cinfo);

	fp = fopen(filename, "rb");
	if(fp == NULL) {
		return -1;
	}

	jpeg_stdio_src(&cinfo, fp);

	jpeg_read_header(&cinfo, TRUE);

	if(cinfo.out_color_space != JCS_RGB) {
		jpeg_destroy_decompress(&cinfo);

		return -1;
	}

	*colorspace = MODJPEG_COLORSPACE_RGB;

	jpeg_start_decompress(&cinfo);

	*width = cinfo.output_width;
	*height = cinfo.output_height;

	int row_stride = cinfo.output_width * cinfo.output_components;

	*len = row_stride * cinfo.output_height;

	unsigned char *buf = (unsigned char *)calloc(row_stride * cinfo.output_height, sizeof(unsigned char));

	JSAMPROW row_pointer[1];

	while (cinfo.output_scanline < cinfo.output_height) {
		row_pointer[0] = &buf[cinfo.output_scanline * row_stride];
		jpeg_read_scanlines(&cinfo, row_pointer, 1);
	}

	jpeg_finish_decompress(&cinfo);
	jpeg_destroy_decompress(&cinfo);

	*buffer = (char *)buf;

	return 0;
}

modjpeg_jpegimage_t *create_image(const char *buffer, size_t len) {
	modjpeg_jpegimage_t *m;
	struct modjpeg_error_mgr jerr;
	struct modjpeg_src_mgr src;
	//jpeg_component_info *component;

	if(buffer == NULL || len == 0) {
		printf("empty buffer\n");
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

	src.buf = (JOCTET *)buffer;
	src.size = len;

	jpeg_read_header(&m->cinfo, TRUE);

	m->coef = jpeg_read_coefficients(&m->cinfo);

	return m;
}

int write_image(modjpeg_jpegimage_t *m, char *filename) {
	FILE *fp;
	char *rebuffer = NULL;
	size_t relen = 0;

	if(m == NULL) {
		printf("jpegimage not given\n");
		return -1;
	}

	fp = fopen(filename, "wb");
	if(fp == NULL) {
		printf("can't open output file\n");
		return -1;
	}

	store_image(m, &rebuffer, &relen);

	printf("restored image of len %ld\n", relen);

	fwrite(rebuffer, 1, relen, fp);

	fclose(fp);

	free(rebuffer);

	return 0;
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

	cinfo.optimize_coding = TRUE;
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