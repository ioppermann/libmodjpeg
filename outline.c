/* rough outline

pixelate(jpegimage, area)
tint(jpegimage, ...)
grayscale(jpegimage)
luminance(jpegimage, ..)

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

0. Logo inkl. Maske laden (Drop-on)
   - unterstütze Farbräume: RGB, YCrCb, GRAYSCALE mit optionalem Alpha
   - Logo immer mit 3 Komponenten RAW bereithalten und Farbraum merken
   - Maske immer mit 1 Komponente RAW bereithalten

   - 3 Helfer:
     - Raw RGBA, YCrCbA oder GRAYSCALEA laden
     - Logo von JPEG laden, ohne Maske aber mit Überblendwert
     - Logo von JPEG laden, Maske von JPEG laden mit Y als Alpha

1. Original-JPEG laden
   - Farbraum bestimmen (unterstützte Farbräume: RGB, YCrCb, GRAYSCALE)
   - Sampling bestimmen

2. Logo inkl. Maske als RGBA bereithalten
   - RGB als JPEG im Speicher ablegen (gleicher Farbraum und gleiches Sampling wie Original-JPEG) => LOGO-JEPG
   - A als Y mit Cr=0 und Cb=0 JPEG im Speicher ablegen (gleicher Farbraum und gleiches Sampling wie Original-JPEG) => MASK-JPEG

3. Kooeffizienten von LOGO-JPEG und MASK-JPEG laden

4. LOGO-JPEG und MASK-JPEG auf Original-JPEG anwenden

*/

// gcc outline.c -o outline -Wall -O2 -lm -ljpeg -I/opt/local/include -L/opt/local/lib

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <math.h>
#include <sys/stat.h>

#include "jpeglib.h"
#include "jerror.h"

#define MJ_DESTBUFFER_CHUNKSIZE		2048

#define MJ_COLORSPACE_RGBA		1
#define MJ_COLORSPACE_RGB		2
#define MJ_COLORSPACE_GRAYSCALE 	3
#define MJ_COLORSPACE_GRAYSCALEA	4
#define MJ_COLORSPACE_YCC		5
#define MJ_COLORSPACE_YCCA		6

#define MJ_ALIGN_TOP			1
#define MJ_ALIGN_BOTTOM			2
#define MJ_ALIGN_LEFT			3
#define MJ_ALIGN_RIGHT			4
#define MJ_ALIGN_CENTER			5

#define MJ_BLEND_NONUNIFORM		-1
#define MJ_BLEND_NONE			0
#define MJ_BLEND_FULL			255


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

typedef struct {
	int h_samp_factor;
	int v_samp_factor;
} mj_samplingfactor_t;

typedef struct {
	int max_h_samp_factor;
	int max_v_samp_factor;

	int h_factor;
	int v_factor;

	mj_samplingfactor_t samp_factor[4];
} mj_sampling_t;

typedef float mj_block_t;

typedef struct {
	int width_in_blocks;
	int height_in_blocks;

	int h_samp_factor;
	int v_samp_factor;

	int nblocks;
	mj_block_t **blocks;
} mj_component_t;

typedef struct {
	struct jpeg_decompress_struct cinfo;
	jvirt_barray_ptr *coef;

	int h_blocks;
	int v_blocks;

	mj_sampling_t sampling;
} mj_jpeg_t;

typedef struct {
	char *raw_image;
	char *raw_alpha;

	size_t raw_width;
	size_t raw_height;
	int raw_colorspace;

	int blend;
	unsigned short offset;

	int image_ncomponents;
	int image_colorspace;

	mj_component_t *image;

	int alpha_ncomponents;
	unsigned short alpha_offset;

	mj_component_t *alpha;
} mj_dropon_t;


// public functions

mj_dropon_t *mj_read_dropon_from_buffer(const char *rawdata, unsigned int colorspace, size_t width, size_t height, short blend);
mj_dropon_t *mj_read_dropon_from_jpeg(const char *filename, const char *mask, short blend);

mj_jpeg_t *mj_read_jpeg_from_buffer(const char *buffer, size_t len);
mj_jpeg_t *mj_read_jpeg_from_file(const char *filename);

int mj_compose(mj_jpeg_t *m, mj_dropon_t *d, int h_align, int v_align, int x_offset, int y_offset);

int mj_write_jpeg_to_buffer(mj_jpeg_t *m, char **buffer, size_t *len);
int mj_write_jpeg_to_file(mj_jpeg_t *m, char *filename);

void mj_destroy_jpeg(mj_jpeg_t *m);
void mj_destroy_dropon(mj_dropon_t *d);


// private functions

int mj_read_droponimage_from_buffer(mj_dropon_t *d, const char *buffer, size_t len);
int mj_read_droponalpha_from_buffer(mj_dropon_t *d, const char *buffer, size_t len);

int mj_update_dropon(mj_dropon_t *d, J_COLOR_SPACE colorspace, mj_sampling_t *s, unsigned short offset);

void mj_destroy_component(mj_component_t *c);

int mj_compose_without_mask(mj_jpeg_t *m, mj_dropon_t *d, int h_offset, int v_offset);
int mj_compose_with_mask(mj_jpeg_t *m, mj_dropon_t *d, int h_offset, int v_offset);

int mj_encode_jpeg_to_buffer(char **buffer, size_t *len, unsigned char *raw_data, int raw_colorspace, J_COLOR_SPACE jpeg_colorspace, mj_sampling_t *s, int width, int height);
int mj_decode_jpeg_to_buffer(char **buffer, size_t *len, int *width, int *height, int want_colorspace, const char *filename);

void mj_convolve(mj_block_t *x, mj_block_t *y, float w, int k, int l);


int main(int argc, char **argv) {
	printf("entering %s\n", __FUNCTION__);

	mj_dropon_t *d;
	d = mj_read_dropon_from_jpeg("./images/logo.jpg", "./images/alpha2.jpg", 128);

	mj_jpeg_t *m;
	m = mj_read_jpeg_from_file("./images/in.jpg");

	mj_compose(m, d, MJ_ALIGN_CENTER, MJ_ALIGN_CENTER, 0, 0);

	mj_write_jpeg_to_file(m, "./images/out.jpg");

	mj_destroy_jpeg(m);
	mj_destroy_dropon(d);

	return 0;
}

mj_dropon_t *mj_read_dropon_from_jpeg(const char *image_file, const char *alpha_file, short blend) {
	printf("entering %s\n", __FUNCTION__);

	char *image_buffer = NULL, *alpha_buffer = NULL, *buffer = NULL;
	int colorspace;
	int image_width = 0, alpha_width = 0;
	int image_height = 0, alpha_height = 0;
	size_t len = 0;

	if(mj_decode_jpeg_to_buffer(&image_buffer, &len, &image_width, &image_height, MJ_COLORSPACE_RGB, image_file) != 0) {
		printf("can't decode jpeg from %s\n", image_file);

		return NULL;
	}

	printf("image: %dx%d pixel, %ld bytes (%ld bytes/pixel)\n", image_width, image_height, len, len / (image_width * image_height));

	if(alpha_file != NULL) {
		if(mj_decode_jpeg_to_buffer(&alpha_buffer, &len, &alpha_width, &alpha_height, MJ_COLORSPACE_GRAYSCALE, alpha_file) != 0) {
			printf("can't decode jpeg from %s\n", alpha_file);

			free(image_buffer);
			return NULL;
		}

		printf("alpha: %dx%d pixel, %ld bytes (%ld bytes/pixel)\n", alpha_width, alpha_height, len, len / (alpha_width * alpha_height));

		if(image_width != alpha_width || image_height != alpha_height) {
			printf("error: dimensions of image and alpha need to be the same!\n");

			free(image_buffer);
			free(alpha_buffer);

			return NULL;
		}

		buffer = (char *)calloc(4 * image_width * image_height, sizeof(char));
		if(buffer == NULL) {
			printf("can't allocate memory for buffer\n");

			free(image_buffer);
			free(alpha_buffer);

			return NULL;
		}

		int v;
		char *t = buffer, *p = image_buffer, *q = alpha_buffer;

		for(v = 0; v < (image_width * image_height); v++) {
			*t++ = *p++;
			*t++ = *p++;
			*t++ = *p++;
			*t++ = *q++;
		}

		colorspace = MJ_COLORSPACE_RGBA; 
	}
	else {
		buffer = image_buffer;
		colorspace = MJ_COLORSPACE_RGB;
	}

	mj_dropon_t *d = mj_read_dropon_from_buffer(buffer, colorspace, image_width, image_height, blend);

	free(image_buffer);

	if(alpha_buffer != NULL) {
		free(alpha_buffer);
		free(buffer);
	}

	return d;
}

int mj_compose(mj_jpeg_t *m, mj_dropon_t *d, int h_align, int v_align, int x_offset, int y_offset) {
	printf("entering %s\n", __FUNCTION__);

	int reload = 0;
	int h_offset = 0, v_offset = 0, offset = 0;
	jpeg_component_info *component_m = NULL;

	if(m == NULL || d == NULL) {
		return 0;
	}

	printf("blend: %d\n", d->blend);

	if(d->blend == MJ_BLEND_NONE) {
		return 0;
	}

	if(d->image != NULL) {
		// is the colorspace the same?
		if(m->cinfo.jpeg_color_space != d->image_colorspace) {
			reload = 1;
		}

		// is the sampling the same?
		int c = 0;
		for(c = 0; c < m->cinfo.num_components; c++) {
			component_m = &m->cinfo.comp_info[c];

			if(component_m->h_samp_factor != d->image[c].h_samp_factor) {
				reload = 1;
			}

			if(component_m->v_samp_factor != d->image[c].v_samp_factor) {
				reload = 1;
			}
		}
	}
	else {
		reload = 1;
	}

	// is the offset the same?
	// calculate needed offset, compare with d->offset

	if(h_align == MJ_ALIGN_LEFT) {
		h_offset = 0;
	}
	else if(h_align == MJ_ALIGN_CENTER) {
		h_offset = m->cinfo.output_width / 2 - d->raw_width / 2;
	}
	else {
		h_offset = m->cinfo.output_width - d->raw_width;
	}

	h_offset += x_offset;

	offset = h_offset % m->sampling.h_factor;
	h_offset = h_offset / m->sampling.h_factor;

	if(v_align == MJ_ALIGN_TOP) {
		v_offset = 0;
	}
	else if(v_align == MJ_ALIGN_CENTER) {
		v_offset = m->cinfo.output_height / 2 - d->raw_height / 2;
	}
	else {
		v_offset = m->cinfo.output_height - d->raw_height;
	}

	v_offset += y_offset;

	offset += m->sampling.h_factor * (v_offset % m->sampling.v_factor);
	v_offset = v_offset / m->sampling.v_factor;

	if(offset != d->offset) {
		reload = 1;
	}

	if(reload == 1) {
		printf("reloading dropon\n");

		mj_update_dropon(d, m->cinfo.jpeg_color_space, &m->sampling, offset);
	}

	if(d->blend != MJ_BLEND_FULL) {
		mj_compose_with_mask(m, d, h_offset, v_offset);
	}
	else {
		mj_compose_without_mask(m, d, h_offset, v_offset);
	}

	return 0;
}

int mj_update_dropon(mj_dropon_t *d, J_COLOR_SPACE colorspace, mj_sampling_t *sampling, unsigned short offset) {
	printf("entering %s\n", __FUNCTION__);

	if(d == NULL) {
		return -1;
	}

	int i, j;

	if(d->image != NULL) {
		for(i = 0; i < d->image_ncomponents; i++) {
			mj_destroy_component(&d->image[i]);
		}

		free(d->image);
		d->image = NULL;
	}	

	if(d->alpha != NULL) {
		for(i = 0; i < d->alpha_ncomponents; i++) {
			mj_destroy_component(&d->alpha[i]);
		}

		free(d->alpha);
		d->alpha = NULL;
	}

	char *buffer = NULL;
	size_t len = 0;

	// hier offset berücksichtigen

	int h_offset = offset % sampling->h_factor;
	int v_offset = offset / sampling->v_factor;

	int raw_width = d->raw_width + sampling->h_factor;
	int raw_height = d->raw_height + sampling->v_factor;
	char *raw_data = (char *)calloc(3 * raw_width * raw_height, sizeof(char));
	if(raw_data == NULL) {
		return -1;
	}

	char *p, *q;

	for(i = 0; i < d->raw_height; i++) {
		p = &raw_data[(i + v_offset) * raw_width * 3 + (h_offset * 3)];
		q = &d->raw_image[i * d->raw_width * 3];

		for(j = 0; j < d->raw_width; j++) {
			*p++ = *q++;
			*p++ = *q++;
			*p++ = *q++;
		}
	}

	mj_encode_jpeg_to_buffer(&buffer, &len, (unsigned char *)raw_data, d->raw_colorspace, colorspace, sampling, raw_width, raw_height);
	printf("encoded len: %ld\n", len);

	mj_read_droponimage_from_buffer(d, buffer, len);
	free(buffer);

	// hier offset berücksichtigen

	for(i = 0; i < d->raw_height; i++) {
		p = &raw_data[(i + v_offset) * raw_width * 3 + (h_offset * 3)];
		q = &d->raw_alpha[i * d->raw_width * 3];

		for(j = 0; j < d->raw_width; j++) {
			*p++ = *q++;
			*p++ = *q++;
			*p++ = *q++;
		}
	}

	mj_encode_jpeg_to_buffer(&buffer, &len, (unsigned char *)raw_data, MJ_COLORSPACE_YCC, JCS_YCbCr, sampling, raw_width, raw_height);
	printf("encoded len: %ld\n", len);

	mj_read_droponalpha_from_buffer(d, buffer, len);
	free(buffer);

	free(raw_data);

	d->offset = offset;

	return 0;
}

int mj_compose_without_mask(mj_jpeg_t *m, mj_dropon_t *d, int h_offset, int v_offset) {
	printf("entering %s\n", __FUNCTION__);

	int c, k, l, i;
	int width_offset = 0, height_offset = 0;
	int width_in_blocks = 0, height_in_blocks = 0;
	struct jpeg_decompress_struct *cinfo_m;
	jpeg_component_info *component_m;
	JBLOCKARRAY blocks_m;
	JCOEFPTR coefs_m;

	mj_component_t *imagecomp;
	mj_block_t *imageblock;

	cinfo_m = &m->cinfo;

	for(c = 0; c < d->image_ncomponents; c++) {
		printf("component %d\n", c);

		component_m = &cinfo_m->comp_info[c];
		imagecomp = &d->image[c];

		width_in_blocks = imagecomp->width_in_blocks;
		height_in_blocks = imagecomp->height_in_blocks;

		width_offset = h_offset * component_m->h_samp_factor;
		height_offset = v_offset * component_m->v_samp_factor;

		/* Die Werte des Logos in das Bild kopieren */
		for(l = 0; l < height_in_blocks; l++) {
			blocks_m = (*cinfo_m->mem->access_virt_barray)((j_common_ptr)&cinfo_m, m->coef[c], height_offset + l, 1, TRUE);

			for(k = 0; k < width_in_blocks; k++) {
				coefs_m = blocks_m[0][width_offset + k];
				imageblock = imagecomp->blocks[height_in_blocks * l + k];

				for(i = 0; i < DCTSIZE2; i += 8) {
					coefs_m[i + 0] = (int)imageblock[i + 0];
					coefs_m[i + 1] = (int)imageblock[i + 1];
					coefs_m[i + 2] = (int)imageblock[i + 2];
					coefs_m[i + 3] = (int)imageblock[i + 3];
					coefs_m[i + 4] = (int)imageblock[i + 4];
					coefs_m[i + 5] = (int)imageblock[i + 5];
					coefs_m[i + 6] = (int)imageblock[i + 6];
					coefs_m[i + 7] = (int)imageblock[i + 7];
				}
			}
		}
	}

	return 0;
}

int mj_compose_with_mask(mj_jpeg_t *m, mj_dropon_t *d, int h_offset, int v_offset) {
	printf("entering %s\n", __FUNCTION__);

	int c, k, l, i;
	int width_offset = 0, height_offset = 0;
	int width_in_blocks = 0, height_in_blocks = 0;
	struct jpeg_decompress_struct *cinfo_m;
	jpeg_component_info *component_m;
	JBLOCKARRAY blocks_m;
	JCOEFPTR coefs_m;
	float X[DCTSIZE2], Y[DCTSIZE2];

	mj_component_t *imagecomp, *alphacomp;
	mj_block_t *imageblock, *alphablock;

	cinfo_m = &m->cinfo;

	for(c = 0; c < d->image_ncomponents; c++) {
		component_m = &cinfo_m->comp_info[c];
		imagecomp = &d->image[c];
		alphacomp = &d->alpha[c];

		width_in_blocks = imagecomp->width_in_blocks;
		height_in_blocks = imagecomp->height_in_blocks;

		width_offset = h_offset * component_m->h_samp_factor;
		height_offset = v_offset * component_m->v_samp_factor;

		/* Die Werte des Logos in das Bild kopieren */
		for(l = 0; l < height_in_blocks; l++) {
			blocks_m = (*cinfo_m->mem->access_virt_barray)((j_common_ptr)&cinfo_m, m->coef[c], height_offset + l, 1, TRUE);

			for(k = 0; k < width_in_blocks; k++) {
				coefs_m = blocks_m[0][width_offset + k];
				alphablock = alphacomp->blocks[height_in_blocks * l + k];
				imageblock = imagecomp->blocks[height_in_blocks * l + k];
/*
				int p, q;

				for(p = 0; p < DCTSIZE; p++) {
					for(q = 0; q < DCTSIZE; q++) {
						printf("%.2f ", imageblock[DCTSIZE * p + q]);
					}
					printf("\n");
				}
*/
				// x = x0 - x1
				printf("component %d (%d,%d): x0 - x1 | ", c, l, k);
				for(i = 0; i < DCTSIZE2; i += 8) {
					X[i + 0] = imageblock[i + 0] - coefs_m[i + 0];
					X[i + 1] = imageblock[i + 1] - coefs_m[i + 1];
					X[i + 2] = imageblock[i + 2] - coefs_m[i + 2];
					X[i + 3] = imageblock[i + 3] - coefs_m[i + 3];
					X[i + 4] = imageblock[i + 4] - coefs_m[i + 4];
					X[i + 5] = imageblock[i + 5] - coefs_m[i + 5];
					X[i + 6] = imageblock[i + 6] - coefs_m[i + 6];
					X[i + 7] = imageblock[i + 7] - coefs_m[i + 7];
				}

				memset(Y, 0, DCTSIZE2 * sizeof(float));

				// y' = w * x (Faltung)
				printf("w * x | ");
				for(i = 0; i < DCTSIZE; i++) {
					mj_convolve(X, Y, alphablock[(i * DCTSIZE) + 0], i, 0);
					mj_convolve(X, Y, alphablock[(i * DCTSIZE) + 1], i, 1);
					mj_convolve(X, Y, alphablock[(i * DCTSIZE) + 2], i, 2);
					mj_convolve(X, Y, alphablock[(i * DCTSIZE) + 3], i, 3);
					mj_convolve(X, Y, alphablock[(i * DCTSIZE) + 4], i, 4);
					mj_convolve(X, Y, alphablock[(i * DCTSIZE) + 5], i, 5);
					mj_convolve(X, Y, alphablock[(i * DCTSIZE) + 6], i, 6);
					mj_convolve(X, Y, alphablock[(i * DCTSIZE) + 7], i, 7);
				}

				// y = x1 + y'
				printf("x1 + y'\n");
				for(i = 0; i < DCTSIZE2; i += 8) {
					coefs_m[i + 0] += (int)Y[i + 0];
					coefs_m[i + 1] += (int)Y[i + 1];
					coefs_m[i + 2] += (int)Y[i + 2];
					coefs_m[i + 3] += (int)Y[i + 3];
					coefs_m[i + 4] += (int)Y[i + 4];
					coefs_m[i + 5] += (int)Y[i + 5];
					coefs_m[i + 6] += (int)Y[i + 6];
					coefs_m[i + 7] += (int)Y[i + 7];
				}
			}
		}
	}

	return 0;
}

int mj_read_droponimage_from_buffer(mj_dropon_t *d, const char *buffer, size_t len) {
	printf("entering %s\n", __FUNCTION__);

	if(d == NULL) {
		return 1;
	}

	mj_jpeg_t *m = mj_read_jpeg_from_buffer(buffer, len);
	if(m == NULL) {
		return 1;
	}

	int c, k, l, i;
	jpeg_component_info *component;
	mj_component_t *comp;
	mj_block_t *b;
	JBLOCKARRAY blocks;
	JCOEFPTR coefs;

	printf("dropon image: %dx%dpx, %d components: ", m->cinfo.output_width, m->cinfo.output_height, m->cinfo.num_components);

	d->image_ncomponents = m->cinfo.num_components;
	d->image_colorspace = m->cinfo.jpeg_color_space;
	d->image = (mj_component_t *)calloc(d->image_ncomponents, sizeof(mj_component_t));

	for(c = 0; c < m->cinfo.num_components; c++) {
		component = &m->cinfo.comp_info[c];
		comp = &d->image[c];

		printf("(%d,%d) ", component->h_samp_factor, component->v_samp_factor);

		comp->h_samp_factor = component->h_samp_factor;
		comp->v_samp_factor = component->v_samp_factor;

		comp->width_in_blocks = m->h_blocks * component->h_samp_factor;
		comp->height_in_blocks = m->v_blocks * component->v_samp_factor;

		comp->nblocks = comp->width_in_blocks * comp->height_in_blocks;
		comp->blocks = (mj_block_t **)calloc(comp->nblocks, sizeof(mj_block_t *));

		printf("%dx%d blocks ", comp->width_in_blocks, comp->height_in_blocks);

		for(l = 0; l < comp->height_in_blocks; l++) {
			blocks = (*m->cinfo.mem->access_virt_barray)((j_common_ptr)&m->cinfo, m->coef[c], l, 1, TRUE);

			for(k = 0; k < comp->width_in_blocks; k++) {
				comp->blocks[comp->height_in_blocks * l + k] = (mj_block_t *)calloc(64, sizeof(mj_block_t));
				b = comp->blocks[comp->height_in_blocks * l + k];

				coefs = blocks[0][k];

/*
				printf("component %d (%d,%d)\n", c, l, k);

				int p, q;
				for(p = 0; p < DCTSIZE; p++) {
					for(q = 0; q < DCTSIZE; q++) {
						printf("%4d ", coefs[DCTSIZE * p + q]);
					}
					printf("\n");
				}
*/
				for(i = 0; i < DCTSIZE2; i += 8) {
					b[i + 0] = (float)coefs[i + 0];
					b[i + 1] = (float)coefs[i + 1];
					b[i + 2] = (float)coefs[i + 2];
					b[i + 3] = (float)coefs[i + 3];
					b[i + 4] = (float)coefs[i + 4];
					b[i + 5] = (float)coefs[i + 5];
					b[i + 6] = (float)coefs[i + 6];
					b[i + 7] = (float)coefs[i + 7];
				}
			}
		}
	}

	printf("\n");

	mj_write_jpeg_to_file(m, "./images/dropon_image.jpg");

	mj_destroy_jpeg(m);

	return 0;
}

int mj_read_droponalpha_from_buffer(mj_dropon_t *d, const char *buffer, size_t len) {
	printf("entering %s\n", __FUNCTION__);

	if(d == NULL) {
		return 1;
	}

	mj_jpeg_t *m = mj_read_jpeg_from_buffer(buffer, len);
	if(m == NULL) {
		return 1;
	}

	int c, k, l, i;
	jpeg_component_info *component;
	mj_component_t *comp;
	mj_block_t *b;
	JBLOCKARRAY blocks;
	JCOEFPTR coefs;

	printf("dropon alpha: %dx%dpx, %d components: ", m->cinfo.output_width, m->cinfo.output_height, m->cinfo.num_components);

	d->alpha_ncomponents = m->cinfo.num_components;
	d->alpha = (mj_component_t *)calloc(d->alpha_ncomponents, sizeof(mj_component_t));

	for(c = 0; c < m->cinfo.num_components; c++) {
		component = &m->cinfo.comp_info[c];
		comp = &d->alpha[c];

		printf("(%d,%d) ", component->h_samp_factor, component->v_samp_factor);

		comp->h_samp_factor = component->h_samp_factor;
		comp->v_samp_factor = component->v_samp_factor;

		comp->width_in_blocks = m->h_blocks * component->h_samp_factor;
		comp->height_in_blocks = m->v_blocks * component->v_samp_factor;

		comp->nblocks = comp->width_in_blocks * comp->height_in_blocks;
		comp->blocks = (mj_block_t **)calloc(comp->nblocks, sizeof(mj_block_t *));

		printf("%dx%d blocks ", comp->width_in_blocks, comp->height_in_blocks);

		for(l = 0; l < comp->height_in_blocks; l++) {
			blocks = (*m->cinfo.mem->access_virt_barray)((j_common_ptr)&m->cinfo, m->coef[c], l, 1, TRUE);

			for(k = 0; k < comp->width_in_blocks; k++) {
				comp->blocks[comp->height_in_blocks * l + k] = (mj_block_t *)calloc(64, sizeof(mj_block_t));
				b = comp->blocks[comp->height_in_blocks * l + k];

				coefs = blocks[0][k];

				coefs[0] += 1024;
/*
				printf("component %d (%d,%d)\n", c, l, k);

				int p, q;
				for(p = 0; p < DCTSIZE; p++) {
					for(q = 0; q < DCTSIZE; q++) {
						printf("%4d ", coefs[DCTSIZE * p + q]);
					}
					printf("\n");
				}
*/
				/*
				// w'(j, i) = w(j, i) * 1/255 * c(i) * c(j) * 1/4
				// Der Faktor 1/4 kommt von den V(i) und V(j)
				// => 1/255 * 1/4 = 1/1020
				*/
				b[0] = (float)coefs[0] * (0.3535534 * 0.3535534 / 1020.0);
				b[1] = (float)coefs[1] * (0.3535534 * 0.5 / 1020.0);
				b[2] = (float)coefs[2] * (0.3535534 * 0.5 / 1020.0);
				b[3] = (float)coefs[3] * (0.3535534 * 0.5 / 1020.0);
				b[4] = (float)coefs[4] * (0.3535534 * 0.5 / 1020.0);
				b[5] = (float)coefs[5] * (0.3535534 * 0.5 / 1020.0);
				b[6] = (float)coefs[6] * (0.3535534 * 0.5 / 1020.0);
				b[7] = (float)coefs[7] * (0.3535534 * 0.5 / 1020.0);

				for(i = 8; i < DCTSIZE2; i += 8) {
					b[i + 0] = (float)coefs[i + 0] * (0.5 * 0.3535534 / 1020.0);
					b[i + 1] = (float)coefs[i + 1] * (0.5 * 0.5 / 1020.0);
					b[i + 2] = (float)coefs[i + 2] * (0.5 * 0.5 / 1020.0);
					b[i + 3] = (float)coefs[i + 3] * (0.5 * 0.5 / 1020.0);
					b[i + 4] = (float)coefs[i + 4] * (0.5 * 0.5 / 1020.0);
					b[i + 5] = (float)coefs[i + 5] * (0.5 * 0.5 / 1020.0);
					b[i + 6] = (float)coefs[i + 6] * (0.5 * 0.5 / 1020.0);
					b[i + 7] = (float)coefs[i + 7] * (0.5 * 0.5 / 1020.0);
				}
			}
		}
	}

	printf("\n");

	mj_write_jpeg_to_file(m, "./images/dropon_alpha.jpg");

	mj_destroy_jpeg(m);

	return 0;
}

int mj_encode_jpeg_to_buffer(char **buffer, size_t *len, unsigned char *data, int colorspace, J_COLOR_SPACE jpeg_colorspace, mj_sampling_t *s, int width, int height) {
	printf("entering %s\n", __FUNCTION__);

	struct jpeg_compress_struct cinfo;
	struct mj_jpeg_error_mgr jerr;
	struct mj_jpeg_dest_mgr dest;
	char jpegerrorbuffer[JMSG_LENGTH_MAX];

	cinfo.err = jpeg_std_error(&jerr.pub);
	jerr.pub.error_exit = mj_jpeg_error_exit;
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
	dest.pub.init_destination = mj_jpeg_init_destination;
	dest.pub.empty_output_buffer = mj_jpeg_empty_output_buffer;
	dest.pub.term_destination = mj_jpeg_term_destination;

	cinfo.image_width = width;
	cinfo.image_height = height;

	if(colorspace == MJ_COLORSPACE_RGB) {
		cinfo.input_components = 3;
		cinfo.in_color_space = JCS_RGB;
	}
	else if(colorspace == MJ_COLORSPACE_YCC) {
		cinfo.input_components = 3;
		cinfo.in_color_space = JCS_YCbCr;
	}
	else if(colorspace == MJ_COLORSPACE_GRAYSCALE) {
		cinfo.input_components = 1;
		cinfo.in_color_space = JCS_GRAYSCALE;
	}
	else {
		printf("invalid colorspace\n");
		jpeg_destroy_compress(&cinfo);
		return -1;
	}

	cinfo.jpeg_color_space = jpeg_colorspace;

	jpeg_set_defaults(&cinfo);

	cinfo.optimize_coding = TRUE;

	if(colorspace == MJ_COLORSPACE_RGB || colorspace == MJ_COLORSPACE_YCC) {
		cinfo.comp_info[0].h_samp_factor = s->samp_factor[0].h_samp_factor;
		cinfo.comp_info[0].v_samp_factor = s->samp_factor[0].v_samp_factor;

		cinfo.comp_info[1].h_samp_factor = s->samp_factor[1].h_samp_factor;;
		cinfo.comp_info[1].v_samp_factor = s->samp_factor[1].v_samp_factor;;

		cinfo.comp_info[2].h_samp_factor = s->samp_factor[2].h_samp_factor;;
		cinfo.comp_info[2].v_samp_factor = s->samp_factor[2].v_samp_factor;;
	}
	else {
		cinfo.comp_info[0].h_samp_factor = s->samp_factor[0].h_samp_factor;;
		cinfo.comp_info[0].v_samp_factor = s->samp_factor[0].v_samp_factor;;
	}

	jpeg_set_quality(&cinfo, 100, TRUE);

	jpeg_start_compress(&cinfo, TRUE);

	int row_stride = cinfo.image_width * cinfo.input_components;

	JSAMPROW row_pointer[1];

	while(cinfo.next_scanline < cinfo.image_height) {
		printf("%d ", cinfo.next_scanline * row_stride);
		row_pointer[0] = &data[cinfo.next_scanline * row_stride];
		jpeg_write_scanlines(&cinfo, row_pointer, 1);
	}

	jpeg_finish_compress(&cinfo);
	jpeg_destroy_compress(&cinfo);

	*buffer = (char *)dest.buf;
	*len = dest.size;

	return 0;
}

void mj_destroy_jpeg(mj_jpeg_t *m) {
	printf("entering %s\n", __FUNCTION__);

	if(m == NULL) {
		return;
	}

	jpeg_destroy_decompress(&m->cinfo);

	m->coef = NULL;

	free(m);

	return;
}

void mj_destroy_dropon(mj_dropon_t *d) {
	printf("entering %s\n", __FUNCTION__);

	if(d == NULL) {
		return;
	}

	if(d->raw_image != NULL) {
		free(d->raw_image);
		d->raw_image = NULL;
	}

	if(d->raw_alpha != NULL) {
		free(d->raw_alpha);
		d->raw_alpha = NULL;
	}

	int i;

	if(d->image != NULL) {
		for(i = 0; i < d->image_ncomponents; i++) {
			mj_destroy_component(&d->image[i]);
		}
		free(d->image);
		d->image = NULL;
	}

	if(d->alpha != NULL) {
		for(i = 0; i < d->alpha_ncomponents; i++) {
			mj_destroy_component(&d->alpha[i]);
		}
		free(d->alpha);
		d->alpha = NULL;
	}

	return;
}

void mj_destroy_component(mj_component_t *c) {
	printf("entering %s\n", __FUNCTION__);

	if(c == NULL) {
		return;
	}

	int i;

	for(i = 0; i < c->nblocks; i++) {
		free(c->blocks[i]);
	}

	free(c->blocks);

	return;
}

mj_dropon_t *mj_read_dropon_from_buffer(const char *raw_data, unsigned int colorspace, size_t width, size_t height, short blend) {
	printf("entering %s\n", __FUNCTION__);

	mj_dropon_t *d;

	if(raw_data == NULL) {
		return NULL;
	}

	if(blend < MJ_BLEND_NONE) {
		blend = MJ_BLEND_NONE;
	}
	else if(blend > MJ_BLEND_FULL) {
		blend = MJ_BLEND_FULL;
	}

	switch(colorspace) {
		case MJ_COLORSPACE_RGB:
		case MJ_COLORSPACE_RGBA:
		case MJ_COLORSPACE_YCC:
		case MJ_COLORSPACE_YCCA:
		case MJ_COLORSPACE_GRAYSCALE:
		case MJ_COLORSPACE_GRAYSCALEA:
			break;
		default:
			printf("unsupported colorspace");
			return NULL;
	}

	d = (mj_dropon_t *)calloc(1, sizeof(mj_dropon_t));
	if(d == NULL) {
		printf("can't allocate dropon");
		return NULL;
	}

	d->raw_width = width;
	d->raw_height = height;
	d->blend = blend;

	// image
	size_t nsamples = 3 * width * height;

	d->raw_image = (char *)calloc(nsamples, sizeof(char));
	if(d->raw_image == NULL) {
		free(d);
		printf("can't allocate buffer");
		return NULL;
	}

	nsamples = 3 * width * height;

	d->raw_alpha = (char *)calloc(nsamples, sizeof(char));
	if(d->raw_alpha == NULL) {
		free(d->raw_image);
		free(d);
		printf("can't allocate buffer");
		return NULL;
	}

	const char *p = raw_data;
	char *pimage = d->raw_image;
	char *palpha = d->raw_alpha;

	size_t v = 0;

	if(colorspace == MJ_COLORSPACE_RGBA || colorspace == MJ_COLORSPACE_YCCA) {
		for(v = 0; v < (width * height); v++) {
			*pimage++ = *p++;
			*pimage++ = *p++;
			*pimage++ = *p++;

			*palpha++ = *p;
			*palpha++ = *p;
			*palpha++ = *p++;
		}

		if(colorspace == MJ_COLORSPACE_RGBA) {
			d->raw_colorspace = MJ_COLORSPACE_RGB;
		}
		else {
			d->raw_colorspace = MJ_COLORSPACE_YCC;
		}

		d->blend = MJ_BLEND_NONUNIFORM;
	}
	else if(colorspace == MJ_COLORSPACE_RGB || colorspace == MJ_COLORSPACE_YCC) {
		for(v = 0; v < (width * height); v++) {
			*pimage++ = *p++;
			*pimage++ = *p++;
			*pimage++ = *p++;

			*palpha++ = (char)d->blend;
			*palpha++ = (char)d->blend;
			*palpha++ = (char)d->blend;
		}

		d->raw_colorspace = colorspace;
	}
	else if(colorspace == MJ_COLORSPACE_GRAYSCALEA) {
		for(v = 0; v < (width * height); v++) {
			*pimage++ = *p;
			*pimage++ = *p;
			*pimage++ = *p++;

			*palpha++ = *p;
			*palpha++ = *p;
			*palpha++ = *p++;
		}

		d->raw_colorspace = MJ_COLORSPACE_GRAYSCALE;

		d->blend = MJ_BLEND_NONUNIFORM;
	}
	else {
		for(v = 0; v < (width * height); v++) {
			*pimage++ = *p;
			*pimage++ = *p;
			*pimage++ = *p++;

			*palpha++ = (char)d->blend;
			*palpha++ = (char)d->blend;
			*palpha++ = (char)d->blend;
		}

		d->raw_colorspace = MJ_COLORSPACE_GRAYSCALE;
	}

	return d;
}

mj_jpeg_t *mj_read_jpeg_from_file(const char *filename) {
	printf("entering %s\n", __FUNCTION__);

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

	mj_jpeg_t *m;

	m = mj_read_jpeg_from_buffer(buffer, len);
	free(buffer);
	if(m == NULL) {
		printf("reading from buffer failed\n");
		return NULL;
	}

	return m;
}

int mj_decode_jpeg_to_buffer(char **buffer, size_t *len, int *width, int *height, int want_colorspace, const char *filename) {
	printf("entering %s\n", __FUNCTION__);

	FILE *fp;
	struct jpeg_decompress_struct cinfo;
	struct mj_jpeg_error_mgr jerr;
	char jpegerrorbuffer[JMSG_LENGTH_MAX];

	cinfo.err = jpeg_std_error(&jerr.pub);
	jerr.pub.error_exit = mj_jpeg_error_exit;
	if(setjmp(jerr.setjmp_buffer)) {
		(*cinfo.err->format_message)((j_common_ptr)&cinfo, jpegerrorbuffer);
		jpeg_destroy_decompress(&cinfo);
		fclose(fp);
		return -1;
	}

	jpeg_create_decompress(&cinfo);

	fp = fopen(filename, "rb");
	if(fp == NULL) {
		jpeg_destroy_decompress(&cinfo);
		return -1;
	}

	jpeg_stdio_src(&cinfo, fp);

	jpeg_read_header(&cinfo, TRUE);

	switch(want_colorspace) {
		case MJ_COLORSPACE_RGB:
			cinfo.out_color_space = JCS_RGB;
			break;
		case MJ_COLORSPACE_YCC:
			cinfo.out_color_space = JCS_YCbCr;
			break;
		case MJ_COLORSPACE_GRAYSCALE:
			cinfo.out_color_space = JCS_GRAYSCALE;
			break;
		default:
			printf("error: unsupported colorspace (%d)\n", want_colorspace);
			jpeg_destroy_decompress(&cinfo);
			return -1;
	}

	jpeg_start_decompress(&cinfo);

	*width = cinfo.output_width;
	*height = cinfo.output_height;

	printf("decode jpeg: %dx%dpx with %d components\n", *width, *height, cinfo.output_components);

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

mj_jpeg_t *mj_read_jpeg_from_buffer(const char *buffer, size_t len) {
	printf("entering %s\n", __FUNCTION__);

	mj_jpeg_t *m;
	struct mj_jpeg_error_mgr jerr;
	struct mj_jpeg_src_mgr src;
	//jpeg_component_info *component;

	if(buffer == NULL || len == 0) {
		printf("empty buffer\n");
		return NULL;
	}

	m = (mj_jpeg_t *)calloc(1, sizeof(mj_jpeg_t));
	if(m == NULL) {
		return NULL;
	}

	m->cinfo.err = jpeg_std_error(&jerr.pub);
	jerr.pub.error_exit = mj_jpeg_error_exit;
	if(setjmp(jerr.setjmp_buffer)) {
		jpeg_destroy_decompress(&m->cinfo);
		free(m);
		return NULL;
	}

	jpeg_create_decompress(&m->cinfo);

	m->cinfo.src = &src.pub;
	src.pub.init_source = mj_jpeg_init_source;
	src.pub.fill_input_buffer = mj_jpeg_fill_input_buffer;
	src.pub.skip_input_data = mj_jpeg_skip_input_data;
	src.pub.resync_to_restart = jpeg_resync_to_restart;
	src.pub.term_source = mj_jpeg_term_source;

	src.buf = (JOCTET *)buffer;
	src.size = len;

	jpeg_read_header(&m->cinfo, TRUE);

	switch(m->cinfo.jpeg_color_space) {
		case JCS_GRAYSCALE:
		case JCS_RGB:
		case JCS_YCbCr:
			break;
		default:
			jpeg_destroy_decompress(&m->cinfo);
			free(m);
			return NULL;
	}

	m->coef = jpeg_read_coefficients(&m->cinfo);

	printf("%dx%dpx, %d components: ", m->cinfo.output_width, m->cinfo.output_height, m->cinfo.num_components);

	m->sampling.max_h_samp_factor = m->cinfo.max_h_samp_factor;
	m->sampling.max_v_samp_factor = m->cinfo.max_v_samp_factor;

	m->sampling.h_factor = (m->sampling.max_h_samp_factor * DCTSIZE);
	m->sampling.v_factor = (m->sampling.max_v_samp_factor * DCTSIZE);

	m->h_blocks = m->cinfo.image_width / m->sampling.h_factor;
	if(m->cinfo.image_width % m->sampling.h_factor != 0) {
		m->h_blocks++;
	}
	m->v_blocks = m->cinfo.image_height / m->sampling.v_factor;
	if(m->cinfo.image_height % m->sampling.v_factor != 0) {
		m->v_blocks++;
	}

	int c, k, l, i;
	int width_in_blocks, height_in_blocks;
	jpeg_component_info *component;
	JBLOCKARRAY blocks;
	JCOEFPTR coefs;

	for(c = 0; c < m->cinfo.num_components; c++) {
		component = &m->cinfo.comp_info[c];

		m->sampling.samp_factor[c].h_samp_factor = component->h_samp_factor;
		m->sampling.samp_factor[c].v_samp_factor = component->v_samp_factor;

		width_in_blocks = m->h_blocks * component->h_samp_factor;
		height_in_blocks = m->v_blocks * component->v_samp_factor;

		printf("(%d,%d) %dx%d blocks ", component->h_samp_factor, component->v_samp_factor, width_in_blocks, height_in_blocks);

		for(l = 0; l < height_in_blocks; l++) {
			blocks = (*m->cinfo.mem->access_virt_barray)((j_common_ptr)&m->cinfo, m->coef[c], l, 1, TRUE);

			for(k = 0; k < width_in_blocks; k++) {
				coefs = blocks[0][k];

				for(i = 0; i < DCTSIZE2; i += 4) {
					coefs[i + 0] *= component->quant_table->quantval[i + 0];
					coefs[i + 1] *= component->quant_table->quantval[i + 1];
					coefs[i + 2] *= component->quant_table->quantval[i + 2];
					coefs[i + 3] *= component->quant_table->quantval[i + 3];
				}
			}
		}
	}

	printf("\n");

	return m;
}

int mj_write_jpeg_to_file(mj_jpeg_t *m, char *filename) {
	printf("entering %s\n", __FUNCTION__);

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

	mj_write_jpeg_to_buffer(m, &rebuffer, &relen);

	printf("restored image of len %ld\n", relen);

	fwrite(rebuffer, 1, relen, fp);

	fclose(fp);

	free(rebuffer);

	return 0;
}

int mj_write_jpeg_to_buffer(mj_jpeg_t *m, char **buffer, size_t *len) {
	printf("entering %s\n", __FUNCTION__);

	struct jpeg_compress_struct cinfo;
	jvirt_barray_ptr *dst_coef_arrays;
	struct mj_jpeg_error_mgr jerr;
	struct mj_jpeg_dest_mgr dest;
	char jpegerrorbuffer[JMSG_LENGTH_MAX];

	cinfo.err = jpeg_std_error(&jerr.pub);
	jerr.pub.error_exit = mj_jpeg_error_exit;
	if(setjmp(jerr.setjmp_buffer)) {
		(*cinfo.err->format_message)((j_common_ptr)&cinfo, jpegerrorbuffer);
		jpeg_destroy_compress(&cinfo);
		if(dest.buf != NULL) {
			free(dest.buf);
		}

		return -1;
	}

	int c, k, l, i;
	int width_in_blocks, height_in_blocks;
	jpeg_component_info *component;
	JBLOCKARRAY blocks;
	JCOEFPTR coefs;

	for(c = 0; c < m->cinfo.num_components; c++) {
		component = &m->cinfo.comp_info[c];

		//printf("(%d,%d)", component->h_samp_factor, component->v_samp_factor);

		width_in_blocks = m->h_blocks * component->h_samp_factor;
		height_in_blocks = m->v_blocks * component->v_samp_factor;

		for(l = 0; l < height_in_blocks; l++) {
			blocks = (*m->cinfo.mem->access_virt_barray)((j_common_ptr)&m->cinfo, m->coef[c], l, 1, TRUE);

			for(k = 0; k < width_in_blocks; k++) {
				coefs = blocks[0][k];

				for(i = 0; i < DCTSIZE2; i += 4) {
					coefs[i + 0] /= component->quant_table->quantval[i + 0];
					coefs[i + 1] /= component->quant_table->quantval[i + 1];
					coefs[i + 2] /= component->quant_table->quantval[i + 2];
					coefs[i + 3] /= component->quant_table->quantval[i + 3];
				}
			}
		}
	}

	jpeg_create_compress(&cinfo);

	cinfo.dest = &dest.pub;
	dest.buf = NULL;
	dest.size = 0;
	dest.pub.init_destination = mj_jpeg_init_destination;
	dest.pub.empty_output_buffer = mj_jpeg_empty_output_buffer;
	dest.pub.term_destination = mj_jpeg_term_destination;

	jpeg_copy_critical_parameters(&m->cinfo, &cinfo);

	cinfo.optimize_coding = TRUE;
	//if((options & MJ_OPTIMIZE) != 0)
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

void mj_convolve(mj_block_t *x, mj_block_t *y, float w, int k, int l) {
	float z[64] = {0, 0, 0, 0, 0, 0, 0, 0,
		       0, 0, 0, 0, 0, 0, 0, 0,
		       0, 0, 0, 0, 0, 0, 0, 0,
		       0, 0, 0, 0, 0, 0, 0, 0,
		       0, 0, 0, 0, 0, 0, 0, 0,
		       0, 0, 0, 0, 0, 0, 0, 0,
		       0, 0, 0, 0, 0, 0, 0, 0,
		       0, 0, 0, 0, 0, 0, 0, 0};

	if(w == 0.0) {
		return;
	}

	switch(l) {
		case 0:
			z[ 0] = (2.0 * x[ 0]);
			z[ 1] = (2.0 * x[ 1]);
			z[ 2] = (2.0 * x[ 2]);
			z[ 3] = (2.0 * x[ 3]);
			z[ 4] = (2.0 * x[ 4]);
			z[ 5] = (2.0 * x[ 5]);
			z[ 6] = (2.0 * x[ 6]);
			z[ 7] = (2.0 * x[ 7]);
			z[ 8] = (2.0 * x[ 8]);
			z[ 9] = (2.0 * x[ 9]);
			z[10] = (2.0 * x[10]);
			z[11] = (2.0 * x[11]);
			z[12] = (2.0 * x[12]);
			z[13] = (2.0 * x[13]);
			z[14] = (2.0 * x[14]);
			z[15] = (2.0 * x[15]);
			z[16] = (2.0 * x[16]);
			z[17] = (2.0 * x[17]);
			z[18] = (2.0 * x[18]);
			z[19] = (2.0 * x[19]);
			z[20] = (2.0 * x[20]);
			z[21] = (2.0 * x[21]);
			z[22] = (2.0 * x[22]);
			z[23] = (2.0 * x[23]);
			z[24] = (2.0 * x[24]);
			z[25] = (2.0 * x[25]);
			z[26] = (2.0 * x[26]);
			z[27] = (2.0 * x[27]);
			z[28] = (2.0 * x[28]);
			z[29] = (2.0 * x[29]);
			z[30] = (2.0 * x[30]);
			z[31] = (2.0 * x[31]);
			z[32] = (2.0 * x[32]);
			z[33] = (2.0 * x[33]);
			z[34] = (2.0 * x[34]);
			z[35] = (2.0 * x[35]);
			z[36] = (2.0 * x[36]);
			z[37] = (2.0 * x[37]);
			z[38] = (2.0 * x[38]);
			z[39] = (2.0 * x[39]);
			z[40] = (2.0 * x[40]);
			z[41] = (2.0 * x[41]);
			z[42] = (2.0 * x[42]);
			z[43] = (2.0 * x[43]);
			z[44] = (2.0 * x[44]);
			z[45] = (2.0 * x[45]);
			z[46] = (2.0 * x[46]);
			z[47] = (2.0 * x[47]);
			z[48] = (2.0 * x[48]);
			z[49] = (2.0 * x[49]);
			z[50] = (2.0 * x[50]);
			z[51] = (2.0 * x[51]);
			z[52] = (2.0 * x[52]);
			z[53] = (2.0 * x[53]);
			z[54] = (2.0 * x[54]);
			z[55] = (2.0 * x[55]);
			z[56] = (2.0 * x[56]);
			z[57] = (2.0 * x[57]);
			z[58] = (2.0 * x[58]);
			z[59] = (2.0 * x[59]);
			z[60] = (2.0 * x[60]);
			z[61] = (2.0 * x[61]);
			z[62] = (2.0 * x[62]);
			z[63] = (2.0 * x[63]);
			break;
		case 1:
			z[ 0] = (M_SQRT2 * x[ 1]);
			z[ 1] = (M_SQRT2 * x[ 0]) + x[ 2];
			z[ 2] = x[ 1] + x[ 3];
			z[ 3] = x[ 2] + x[ 4];
			z[ 4] = x[ 3] + x[ 5];
			z[ 5] = x[ 4] + x[ 6];
			z[ 6] = x[ 5] + x[ 7];
			z[ 7] = x[ 6];
			z[ 8] = (M_SQRT2 * x[ 9]);
			z[ 9] = (M_SQRT2 * x[ 8]) + x[10];
			z[10] = x[ 9] + x[11];
			z[11] = x[10] + x[12];
			z[12] = x[11] + x[13];
			z[13] = x[12] + x[14];
			z[14] = x[13] + x[15];
			z[15] = x[14];
			z[16] = (M_SQRT2 * x[17]);
			z[17] = (M_SQRT2 * x[16]) + x[18];
			z[18] = x[17] + x[19];
			z[19] = x[18] + x[20];
			z[20] = x[19] + x[21];
			z[21] = x[20] + x[22];
			z[22] = x[21] + x[23];
			z[23] = x[22];
			z[24] = (M_SQRT2 * x[25]);
			z[25] = (M_SQRT2 * x[24]) + x[26];
			z[26] = x[25] + x[27];
			z[27] = x[26] + x[28];
			z[28] = x[27] + x[29];
			z[29] = x[28] + x[30];
			z[30] = x[29] + x[31];
			z[31] = x[30];
			z[32] = (M_SQRT2 * x[33]);
			z[33] = (M_SQRT2 * x[32]) + x[34];
			z[34] = x[33] + x[35];
			z[35] = x[34] + x[36];
			z[36] = x[35] + x[37];
			z[37] = x[36] + x[38];
			z[38] = x[37] + x[39];
			z[39] = x[38];
			z[40] = (M_SQRT2 * x[41]);
			z[41] = (M_SQRT2 * x[40]) + x[42];
			z[42] = x[41] + x[43];
			z[43] = x[42] + x[44];
			z[44] = x[43] + x[45];
			z[45] = x[44] + x[46];
			z[46] = x[45] + x[47];
			z[47] = x[46];
			z[48] = (M_SQRT2 * x[49]);
			z[49] = (M_SQRT2 * x[48]) + x[50];
			z[50] = x[49] + x[51];
			z[51] = x[50] + x[52];
			z[52] = x[51] + x[53];
			z[53] = x[52] + x[54];
			z[54] = x[53] + x[55];
			z[55] = x[54];
			z[56] = (M_SQRT2 * x[57]);
			z[57] = (M_SQRT2 * x[56]) + x[58];
			z[58] = x[57] + x[59];
			z[59] = x[58] + x[60];
			z[60] = x[59] + x[61];
			z[61] = x[60] + x[62];
			z[62] = x[61] + x[63];
			z[63] = x[62];
			break;
		case 2:
			z[ 0] = (M_SQRT2 * x[ 2]);
			z[ 1] = x[ 1] + x[ 3];
			z[ 2] = (M_SQRT2 * x[ 0]) + x[ 4];
			z[ 3] = x[ 1] + x[ 5];
			z[ 4] = x[ 2] + x[ 6];
			z[ 5] = x[ 3] + x[ 7];
			z[ 6] = x[ 4];
			z[ 7] = x[ 5] - x[ 7];
			z[ 8] = (M_SQRT2 * x[10]);
			z[ 9] = x[ 9] + x[11];
			z[10] = (M_SQRT2 * x[ 8]) + x[12];
			z[11] = x[ 9] + x[13];
			z[12] = x[10] + x[14];
			z[13] = x[11] + x[15];
			z[14] = x[12];
			z[15] = x[13] - x[15];
			z[16] = (M_SQRT2 * x[18]);
			z[17] = x[17] + x[19];
			z[18] = (M_SQRT2 * x[16]) + x[20];
			z[19] = x[17] + x[21];
			z[20] = x[18] + x[22];
			z[21] = x[19] + x[23];
			z[22] = x[20];
			z[23] = x[21] - x[23];
			z[24] = (M_SQRT2 * x[26]);
			z[25] = x[25] + x[27];
			z[26] = (M_SQRT2 * x[24]) + x[28];
			z[27] = x[25] + x[29];
			z[28] = x[26] + x[30];
			z[29] = x[27] + x[31];
			z[30] = x[28];
			z[31] = x[29] - x[31];
			z[32] = (M_SQRT2 * x[34]);
			z[33] = x[33] + x[35];
			z[34] = (M_SQRT2 * x[32]) + x[36];
			z[35] = x[33] + x[37];
			z[36] = x[34] + x[38];
			z[37] = x[35] + x[39];
			z[38] = x[36];
			z[39] = x[37] - x[39];
			z[40] = (M_SQRT2 * x[42]);
			z[41] = x[41] + x[43];
			z[42] = (M_SQRT2 * x[40]) + x[44];
			z[43] = x[41] + x[45];
			z[44] = x[42] + x[46];
			z[45] = x[43] + x[47];
			z[46] = x[44];
			z[47] = x[45] - x[47];
			z[48] = (M_SQRT2 * x[50]);
			z[49] = x[49] + x[51];
			z[50] = (M_SQRT2 * x[48]) + x[52];
			z[51] = x[49] + x[53];
			z[52] = x[50] + x[54];
			z[53] = x[51] + x[55];
			z[54] = x[52];
			z[55] = x[53] - x[55];
			z[56] = (M_SQRT2 * x[58]);
			z[57] = x[57] + x[59];
			z[58] = (M_SQRT2 * x[56]) + x[60];
			z[59] = x[57] + x[61];
			z[60] = x[58] + x[62];
			z[61] = x[59] + x[63];
			z[62] = x[60];
			z[63] = x[61] - x[63];
			break;
		case 3:
			z[ 0] = (M_SQRT2 * x[ 3]);
			z[ 1] = x[ 2] + x[ 4];
			z[ 2] = x[ 1] + x[ 5];
			z[ 3] = (M_SQRT2 * x[ 0]) + x[ 6];
			z[ 4] = x[ 1] + x[ 7];
			z[ 5] = x[ 2];
			z[ 6] = x[ 3] - x[ 7];
			z[ 7] = x[ 4] - x[ 6];
			z[ 8] = (M_SQRT2 * x[11]);
			z[ 9] = x[10] + x[12];
			z[10] = x[ 9] + x[13];
			z[11] = (M_SQRT2 * x[ 8]) + x[14];
			z[12] = x[ 9] + x[15];
			z[13] = x[10];
			z[14] = x[11] - x[15];
			z[15] = x[12] - x[14];
			z[16] = (M_SQRT2 * x[19]);
			z[17] = x[18] + x[20];
			z[18] = x[17] + x[21];
			z[19] = (M_SQRT2 * x[16]) + x[22];
			z[20] = x[17] + x[23];
			z[21] = x[18];
			z[22] = x[19] - x[23];
			z[23] = x[20] - x[22];
			z[24] = (M_SQRT2 * x[27]);
			z[25] = x[26] + x[28];
			z[26] = x[25] + x[29];
			z[27] = (M_SQRT2 * x[24]) + x[30];
			z[28] = x[25] + x[31];
			z[29] = x[26];
			z[30] = x[27] - x[31];
			z[31] = x[28] - x[30];
			z[32] = (M_SQRT2 * x[35]);
			z[33] = x[34] + x[36];
			z[34] = x[33] + x[37];
			z[35] = (M_SQRT2 * x[32]) + x[38];
			z[36] = x[33] + x[39];
			z[37] = x[34];
			z[38] = x[35] - x[39];
			z[39] = x[36] - x[38];
			z[40] = (M_SQRT2 * x[43]);
			z[41] = x[42] + x[44];
			z[42] = x[41] + x[45];
			z[43] = (M_SQRT2 * x[40]) + x[46];
			z[44] = x[41] + x[47];
			z[45] = x[42];
			z[46] = x[43] - x[47];
			z[47] = x[44] - x[46];
			z[48] = (M_SQRT2 * x[51]);
			z[49] = x[50] + x[52];
			z[50] = x[49] + x[53];
			z[51] = (M_SQRT2 * x[48]) + x[54];
			z[52] = x[49] + x[55];
			z[53] = x[50];
			z[54] = x[51] - x[55];
			z[55] = x[52] - x[54];
			z[56] = (M_SQRT2 * x[59]);
			z[57] = x[58] + x[60];
			z[58] = x[57] + x[61];
			z[59] = (M_SQRT2 * x[56]) + x[62];
			z[60] = x[57] + x[63];
			z[61] = x[58];
			z[62] = x[59] - x[63];
			z[63] = x[60] - x[62];
			break;
		case 4:
			z[ 0] = (M_SQRT2 * x[ 4]);
			z[ 1] = x[ 3] + x[ 5];
			z[ 2] = x[ 2] + x[ 6];
			z[ 3] = x[ 1] + x[ 7];
			z[ 4] = (M_SQRT2 * x[ 0]);
			z[ 5] = x[ 1] - x[ 7];
			z[ 6] = x[ 2] - x[ 6];
			z[ 7] = x[ 3] - x[ 5];
			z[ 8] = (M_SQRT2 * x[12]);
			z[ 9] = x[11] + x[13];
			z[10] = x[10] + x[14];
			z[11] = x[ 9] + x[15];
			z[12] = (M_SQRT2 * x[ 8]);
			z[13] = x[ 9] - x[15];
			z[14] = x[10] - x[14];
			z[15] = x[11] - x[13];
			z[16] = (M_SQRT2 * x[20]);
			z[17] = x[19] + x[21];
			z[18] = x[18] + x[22];
			z[19] = x[17] + x[23];
			z[20] = (M_SQRT2 * x[16]);
			z[21] = x[17] - x[23];
			z[22] = x[18] - x[22];
			z[23] = x[19] - x[21];
			z[24] = (M_SQRT2 * x[28]);
			z[25] = x[27] + x[29];
			z[26] = x[26] + x[30];
			z[27] = x[25] + x[31];
			z[28] = (M_SQRT2 * x[24]);
			z[29] = x[25] - x[31];
			z[30] = x[26] - x[30];
			z[31] = x[27] - x[29];
			z[32] = (M_SQRT2 * x[36]);
			z[33] = x[35] + x[37];
			z[34] = x[34] + x[38];
			z[35] = x[33] + x[39];
			z[36] = (M_SQRT2 * x[32]);
			z[37] = x[33] - x[39];
			z[38] = x[34] - x[38];
			z[39] = x[35] - x[37];
			z[40] = (M_SQRT2 * x[44]);
			z[41] = x[43] + x[45];
			z[42] = x[42] + x[46];
			z[43] = x[41] + x[47];
			z[44] = (M_SQRT2 * x[40]);
			z[45] = x[41] - x[47];
			z[46] = x[42] - x[46];
			z[47] = x[43] - x[45];
			z[48] = (M_SQRT2 * x[52]);
			z[49] = x[51] + x[53];
			z[50] = x[50] + x[54];
			z[51] = x[49] + x[55];
			z[52] = (M_SQRT2 * x[48]);
			z[53] = x[49] - x[55];
			z[54] = x[50] - x[54];
			z[55] = x[51] - x[53];
			z[56] = (M_SQRT2 * x[60]);
			z[57] = x[59] + x[61];
			z[58] = x[58] + x[62];
			z[59] = x[57] + x[63];
			z[60] = (M_SQRT2 * x[56]);
			z[61] = x[57] - x[63];
			z[62] = x[58] - x[62];
			z[63] = x[59] - x[61];
			break;
		case 5:
			z[ 0] = (M_SQRT2 * x[ 5]);
			z[ 1] = x[ 4] + x[ 6];
			z[ 2] = x[ 3] + x[ 7];
			z[ 3] = x[ 2];
			z[ 4] = x[ 1] - x[ 7];
			z[ 5] = (M_SQRT2 * x[ 0]) - x[ 6];
			z[ 6] = x[ 1] - x[ 5];
			z[ 7] = x[ 2] - x[ 4];
			z[ 8] = (M_SQRT2 * x[13]);
			z[ 9] = x[12] + x[14];
			z[10] = x[11] + x[15];
			z[11] = x[10];
			z[12] = x[ 9] - x[15];
			z[13] = (M_SQRT2 * x[ 8]) - x[14];
			z[14] = x[ 9] - x[13];
			z[15] = x[10] - x[12];
			z[16] = (M_SQRT2 * x[21]);
			z[17] = x[20] + x[22];
			z[18] = x[19] + x[23];
			z[19] = x[18];
			z[20] = x[17] - x[23];
			z[21] = (M_SQRT2 * x[16]) - x[22];
			z[22] = x[17] - x[21];
			z[23] = x[18] - x[20];
			z[24] = (M_SQRT2 * x[29]);
			z[25] = x[28] + x[30];
			z[26] = x[27] + x[31];
			z[27] = x[26];
			z[28] = x[25] - x[31];
			z[29] = (M_SQRT2 * x[24]) - x[30];
			z[30] = x[25] - x[29];
			z[31] = x[26] - x[28];
			z[32] = (M_SQRT2 * x[37]);
			z[33] = x[36] + x[38];
			z[34] = x[35] + x[39];
			z[35] = x[34];
			z[36] = x[33] - x[39];
			z[37] = (M_SQRT2 * x[32]) - x[38];
			z[38] = x[33] - x[37];
			z[39] = x[34] - x[36];
			z[40] = (M_SQRT2 * x[45]);
			z[41] = x[44] + x[46];
			z[42] = x[43] + x[47];
			z[43] = x[42];
			z[44] = x[41] - x[47];
			z[45] = (M_SQRT2 * x[40]) - x[46];
			z[46] = x[41] - x[45];
			z[47] = x[42] - x[44];
			z[48] = (M_SQRT2 * x[53]);
			z[49] = x[52] + x[54];
			z[50] = x[51] + x[55];
			z[51] = x[50];
			z[52] = x[49] - x[55];
			z[53] = (M_SQRT2 * x[48]) - x[54];
			z[54] = x[49] - x[53];
			z[55] = x[50] - x[52];
			z[56] = (M_SQRT2 * x[61]);
			z[57] = x[60] + x[62];
			z[58] = x[59] + x[63];
			z[59] = x[58];
			z[60] = x[57] - x[63];
			z[61] = (M_SQRT2 * x[56]) - x[62];
			z[62] = x[57] - x[61];
			z[63] = x[58] - x[60];
			break;
		case 6:
			z[ 0] = (M_SQRT2 * x[ 6]);
			z[ 1] = x[ 5] + x[ 7];
			z[ 2] = x[ 4];
			z[ 3] = x[ 3] - x[ 7];
			z[ 4] = x[ 2] - x[ 6];
			z[ 5] = x[ 1] - x[ 5];
			z[ 6] = (M_SQRT2 * x[ 0]) - x[ 4];
			z[ 7] = x[ 1] - x[ 3];
			z[ 8] = (M_SQRT2 * x[14]);
			z[ 9] = x[13] + x[15];
			z[10] = x[12];
			z[11] = x[11] - x[15];
			z[12] = x[10] - x[14];
			z[13] = x[ 9] - x[13];
			z[14] = (M_SQRT2 * x[ 8]) - x[12];
			z[15] = x[ 9] - x[11];
			z[16] = (M_SQRT2 * x[22]);
			z[17] = x[21] + x[23];
			z[18] = x[20];
			z[19] = x[19] - x[23];
			z[20] = x[18] - x[22];
			z[21] = x[17] - x[21];
			z[22] = (M_SQRT2 * x[16]) - x[20];
			z[23] = x[17] - x[19];
			z[24] = (M_SQRT2 * x[30]);
			z[25] = x[29] + x[31];
			z[26] = x[28];
			z[27] = x[27] - x[31];
			z[28] = x[26] - x[30];
			z[29] = x[25] - x[29];
			z[30] = (M_SQRT2 * x[24]) - x[28];
			z[31] = x[25] - x[27];
			z[32] = (M_SQRT2 * x[38]);
			z[33] = x[37] + x[39];
			z[34] = x[36];
			z[35] = x[35] - x[39];
			z[36] = x[34] - x[38];
			z[37] = x[33] - x[37];
			z[38] = (M_SQRT2 * x[32]) - x[36];
			z[39] = x[33] - x[35];
			z[40] = (M_SQRT2 * x[46]);
			z[41] = x[45] + x[47];
			z[42] = x[44];
			z[43] = x[43] - x[47];
			z[44] = x[42] - x[46];
			z[45] = x[41] - x[45];
			z[46] = (M_SQRT2 * x[40]) - x[44];
			z[47] = x[41] - x[43];
			z[48] = (M_SQRT2 * x[54]);
			z[49] = x[53] + x[55];
			z[50] = x[52];
			z[51] = x[51] - x[55];
			z[52] = x[50] - x[54];
			z[53] = x[49] - x[53];
			z[54] = (M_SQRT2 * x[48]) - x[52];
			z[55] = x[49] - x[51];
			z[56] = (M_SQRT2 * x[62]);
			z[57] = x[61] + x[63];
			z[58] = x[60];
			z[59] = x[59] - x[63];
			z[60] = x[58] - x[62];
			z[61] = x[57] - x[61];
			z[62] = (M_SQRT2 * x[56]) - x[60];
			z[63] = x[57] - x[59];
			break;
		case 7:
			z[ 0] = (M_SQRT2 * x[ 7]);
			z[ 1] = x[ 6];
			z[ 2] = x[ 5] - x[ 7];
			z[ 3] = x[ 4] - x[ 6];
			z[ 4] = x[ 3] - x[ 5];
			z[ 5] = x[ 2] - x[ 4];
			z[ 6] = x[ 1] - x[ 3];
			z[ 7] = (M_SQRT2 * x[ 0]) - x[ 2];
			z[ 8] = (M_SQRT2 * x[15]);
			z[ 9] = x[14];
			z[10] = x[13] - x[15];
			z[11] = x[12] - x[14];
			z[12] = x[11] - x[13];
			z[13] = x[10] - x[12];
			z[14] = x[ 9] - x[11];
			z[15] = (M_SQRT2 * x[ 8]) - x[10];
			z[16] = (M_SQRT2 * x[23]);
			z[17] = x[22];
			z[18] = x[21] - x[23];
			z[19] = x[20] - x[22];
			z[20] = x[19] - x[21];
			z[21] = x[18] - x[20];
			z[22] = x[17] - x[19];
			z[23] = (M_SQRT2 * x[16]) - x[18];
			z[24] = (M_SQRT2 * x[31]);
			z[25] = x[30];
			z[26] = x[29] - x[31];
			z[27] = x[28] - x[30];
			z[28] = x[27] - x[29];
			z[29] = x[26] - x[28];
			z[30] = x[25] - x[27];
			z[31] = (M_SQRT2 * x[24]) - x[26];
			z[32] = (M_SQRT2 * x[39]);
			z[33] = x[38];
			z[34] = x[37] - x[39];
			z[35] = x[36] - x[38];
			z[36] = x[35] - x[37];
			z[37] = x[34] - x[36];
			z[38] = x[33] - x[35];
			z[39] = (M_SQRT2 * x[32]) - x[34];
			z[40] = (M_SQRT2 * x[47]);
			z[41] = x[46];
			z[42] = x[45] - x[47];
			z[43] = x[44] - x[46];
			z[44] = x[43] - x[45];
			z[45] = x[42] - x[44];
			z[46] = x[41] - x[43];
			z[47] = (M_SQRT2 * x[40]) - x[42];
			z[48] = (M_SQRT2 * x[55]);
			z[49] = x[54];
			z[50] = x[53] - x[55];
			z[51] = x[52] - x[54];
			z[52] = x[51] - x[53];
			z[53] = x[50] - x[52];
			z[54] = x[49] - x[51];
			z[55] = (M_SQRT2 * x[48]) - x[50];
			z[56] = (M_SQRT2 * x[63]);
			z[57] = x[62];
			z[58] = x[61] - x[63];
			z[59] = x[60] - x[62];
			z[60] = x[59] - x[61];
			z[61] = x[58] - x[60];
			z[62] = x[57] - x[59];
			z[63] = (M_SQRT2 * x[56]) - x[58];
			break;
	}

	switch(k) {
		case 0:
			y[ 0] += ((2.0 * z[ 0])) * w;
			y[ 1] += ((2.0 * z[ 1])) * w;
			y[ 2] += ((2.0 * z[ 2])) * w;
			y[ 3] += ((2.0 * z[ 3])) * w;
			y[ 4] += ((2.0 * z[ 4])) * w;
			y[ 5] += ((2.0 * z[ 5])) * w;
			y[ 6] += ((2.0 * z[ 6])) * w;
			y[ 7] += ((2.0 * z[ 7])) * w;
			y[ 8] += ((2.0 * z[ 8])) * w;
			y[ 9] += ((2.0 * z[ 9])) * w;
			y[10] += ((2.0 * z[10])) * w;
			y[11] += ((2.0 * z[11])) * w;
			y[12] += ((2.0 * z[12])) * w;
			y[13] += ((2.0 * z[13])) * w;
			y[14] += ((2.0 * z[14])) * w;
			y[15] += ((2.0 * z[15])) * w;
			y[16] += ((2.0 * z[16])) * w;
			y[17] += ((2.0 * z[17])) * w;
			y[18] += ((2.0 * z[18])) * w;
			y[19] += ((2.0 * z[19])) * w;
			y[20] += ((2.0 * z[20])) * w;
			y[21] += ((2.0 * z[21])) * w;
			y[22] += ((2.0 * z[22])) * w;
			y[23] += ((2.0 * z[23])) * w;
			y[24] += ((2.0 * z[24])) * w;
			y[25] += ((2.0 * z[25])) * w;
			y[26] += ((2.0 * z[26])) * w;
			y[27] += ((2.0 * z[27])) * w;
			y[28] += ((2.0 * z[28])) * w;
			y[29] += ((2.0 * z[29])) * w;
			y[30] += ((2.0 * z[30])) * w;
			y[31] += ((2.0 * z[31])) * w;
			y[32] += ((2.0 * z[32])) * w;
			y[33] += ((2.0 * z[33])) * w;
			y[34] += ((2.0 * z[34])) * w;
			y[35] += ((2.0 * z[35])) * w;
			y[36] += ((2.0 * z[36])) * w;
			y[37] += ((2.0 * z[37])) * w;
			y[38] += ((2.0 * z[38])) * w;
			y[39] += ((2.0 * z[39])) * w;
			y[40] += ((2.0 * z[40])) * w;
			y[41] += ((2.0 * z[41])) * w;
			y[42] += ((2.0 * z[42])) * w;
			y[43] += ((2.0 * z[43])) * w;
			y[44] += ((2.0 * z[44])) * w;
			y[45] += ((2.0 * z[45])) * w;
			y[46] += ((2.0 * z[46])) * w;
			y[47] += ((2.0 * z[47])) * w;
			y[48] += ((2.0 * z[48])) * w;
			y[49] += ((2.0 * z[49])) * w;
			y[50] += ((2.0 * z[50])) * w;
			y[51] += ((2.0 * z[51])) * w;
			y[52] += ((2.0 * z[52])) * w;
			y[53] += ((2.0 * z[53])) * w;
			y[54] += ((2.0 * z[54])) * w;
			y[55] += ((2.0 * z[55])) * w;
			y[56] += ((2.0 * z[56])) * w;
			y[57] += ((2.0 * z[57])) * w;
			y[58] += ((2.0 * z[58])) * w;
			y[59] += ((2.0 * z[59])) * w;
			y[60] += ((2.0 * z[60])) * w;
			y[61] += ((2.0 * z[61])) * w;
			y[62] += ((2.0 * z[62])) * w;
			y[63] += ((2.0 * z[63])) * w;
			break;
		case 1:
			y[ 0] += ((M_SQRT2 * z[ 8])) * w;
			y[ 1] += ((M_SQRT2 * z[ 9])) * w;
			y[ 2] += ((M_SQRT2 * z[10])) * w;
			y[ 3] += ((M_SQRT2 * z[11])) * w;
			y[ 4] += ((M_SQRT2 * z[12])) * w;
			y[ 5] += ((M_SQRT2 * z[13])) * w;
			y[ 6] += ((M_SQRT2 * z[14])) * w;
			y[ 7] += ((M_SQRT2 * z[15])) * w;
			y[ 8] += ((M_SQRT2 * z[ 0]) + z[16]) * w;
			y[ 9] += ((M_SQRT2 * z[ 1]) + z[17]) * w;
			y[10] += ((M_SQRT2 * z[ 2]) + z[18]) * w;
			y[11] += ((M_SQRT2 * z[ 3]) + z[19]) * w;
			y[12] += ((M_SQRT2 * z[ 4]) + z[20]) * w;
			y[13] += ((M_SQRT2 * z[ 5]) + z[21]) * w;
			y[14] += ((M_SQRT2 * z[ 6]) + z[22]) * w;
			y[15] += ((M_SQRT2 * z[ 7]) + z[23]) * w;
			y[16] += (z[ 8] + z[24]) * w;
			y[17] += (z[ 9] + z[25]) * w;
			y[18] += (z[10] + z[26]) * w;
			y[19] += (z[11] + z[27]) * w;
			y[20] += (z[12] + z[28]) * w;
			y[21] += (z[13] + z[29]) * w;
			y[22] += (z[14] + z[30]) * w;
			y[23] += (z[15] + z[31]) * w;
			y[24] += (z[16] + z[32]) * w;
			y[25] += (z[17] + z[33]) * w;
			y[26] += (z[18] + z[34]) * w;
			y[27] += (z[19] + z[35]) * w;
			y[28] += (z[20] + z[36]) * w;
			y[29] += (z[21] + z[37]) * w;
			y[30] += (z[22] + z[38]) * w;
			y[31] += (z[23] + z[39]) * w;
			y[32] += (z[24] + z[40]) * w;
			y[33] += (z[25] + z[41]) * w;
			y[34] += (z[26] + z[42]) * w;
			y[35] += (z[27] + z[43]) * w;
			y[36] += (z[28] + z[44]) * w;
			y[37] += (z[29] + z[45]) * w;
			y[38] += (z[30] + z[46]) * w;
			y[39] += (z[31] + z[47]) * w;
			y[40] += (z[32] + z[48]) * w;
			y[41] += (z[33] + z[49]) * w;
			y[42] += (z[34] + z[50]) * w;
			y[43] += (z[35] + z[51]) * w;
			y[44] += (z[36] + z[52]) * w;
			y[45] += (z[37] + z[53]) * w;
			y[46] += (z[38] + z[54]) * w;
			y[47] += (z[39] + z[55]) * w;
			y[48] += (z[40] + z[56]) * w;
			y[49] += (z[41] + z[57]) * w;
			y[50] += (z[42] + z[58]) * w;
			y[51] += (z[43] + z[59]) * w;
			y[52] += (z[44] + z[60]) * w;
			y[53] += (z[45] + z[61]) * w;
			y[54] += (z[46] + z[62]) * w;
			y[55] += (z[47] + z[63]) * w;
			y[56] += (z[48]) * w;
			y[57] += (z[49]) * w;
			y[58] += (z[50]) * w;
			y[59] += (z[51]) * w;
			y[60] += (z[52]) * w;
			y[61] += (z[53]) * w;
			y[62] += (z[54]) * w;
			y[63] += (z[55]) * w;
			break;
		case 2:
			y[ 0] += ((M_SQRT2 * z[16])) * w;
			y[ 1] += ((M_SQRT2 * z[17])) * w;
			y[ 2] += ((M_SQRT2 * z[18])) * w;
			y[ 3] += ((M_SQRT2 * z[19])) * w;
			y[ 4] += ((M_SQRT2 * z[20])) * w;
			y[ 5] += ((M_SQRT2 * z[21])) * w;
			y[ 6] += ((M_SQRT2 * z[22])) * w;
			y[ 7] += ((M_SQRT2 * z[23])) * w;
			y[ 8] += (z[ 8] + z[24]) * w;
			y[ 9] += (z[ 9] + z[25]) * w;
			y[10] += (z[10] + z[26]) * w;
			y[11] += (z[11] + z[27]) * w;
			y[12] += (z[12] + z[28]) * w;
			y[13] += (z[13] + z[29]) * w;
			y[14] += (z[14] + z[30]) * w;
			y[15] += (z[15] + z[31]) * w;
			y[16] += ((M_SQRT2 * z[ 0]) + z[32]) * w;
			y[17] += ((M_SQRT2 * z[ 1]) + z[33]) * w;
			y[18] += ((M_SQRT2 * z[ 2]) + z[34]) * w;
			y[19] += ((M_SQRT2 * z[ 3]) + z[35]) * w;
			y[20] += ((M_SQRT2 * z[ 4]) + z[36]) * w;
			y[21] += ((M_SQRT2 * z[ 5]) + z[37]) * w;
			y[22] += ((M_SQRT2 * z[ 6]) + z[38]) * w;
			y[23] += ((M_SQRT2 * z[ 7]) + z[39]) * w;
			y[24] += (z[ 8] + z[40]) * w;
			y[25] += (z[ 9] + z[41]) * w;
			y[26] += (z[10] + z[42]) * w;
			y[27] += (z[11] + z[43]) * w;
			y[28] += (z[12] + z[44]) * w;
			y[29] += (z[13] + z[45]) * w;
			y[30] += (z[14] + z[46]) * w;
			y[31] += (z[15] + z[47]) * w;
			y[32] += (z[16] + z[48]) * w;
			y[33] += (z[17] + z[49]) * w;
			y[34] += (z[18] + z[50]) * w;
			y[35] += (z[19] + z[51]) * w;
			y[36] += (z[20] + z[52]) * w;
			y[37] += (z[21] + z[53]) * w;
			y[38] += (z[22] + z[54]) * w;
			y[39] += (z[23] + z[55]) * w;
			y[40] += (z[24] + z[56]) * w;
			y[41] += (z[25] + z[57]) * w;
			y[42] += (z[26] + z[58]) * w;
			y[43] += (z[27] + z[59]) * w;
			y[44] += (z[28] + z[60]) * w;
			y[45] += (z[29] + z[61]) * w;
			y[46] += (z[30] + z[62]) * w;
			y[47] += (z[31] + z[63]) * w;
			y[48] += (z[32]) * w;
			y[49] += (z[33]) * w;
			y[50] += (z[34]) * w;
			y[51] += (z[35]) * w;
			y[52] += (z[36]) * w;
			y[53] += (z[37]) * w;
			y[54] += (z[38]) * w;
			y[55] += (z[39]) * w;
			y[56] += (z[40] - z[56]) * w;
			y[57] += (z[41] - z[57]) * w;
			y[58] += (z[42] - z[58]) * w;
			y[59] += (z[43] - z[59]) * w;
			y[60] += (z[44] - z[60]) * w;
			y[61] += (z[45] - z[61]) * w;
			y[62] += (z[46] - z[62]) * w;
			y[63] += (z[47] - z[63]) * w;
			break;
		case 3:
			y[ 0] += ((M_SQRT2 * z[24])) * w;
			y[ 1] += ((M_SQRT2 * z[25])) * w;
			y[ 2] += ((M_SQRT2 * z[26])) * w;
			y[ 3] += ((M_SQRT2 * z[27])) * w;
			y[ 4] += ((M_SQRT2 * z[28])) * w;
			y[ 5] += ((M_SQRT2 * z[29])) * w;
			y[ 6] += ((M_SQRT2 * z[30])) * w;
			y[ 7] += ((M_SQRT2 * z[31])) * w;
			y[ 8] += (z[16] + z[32]) * w;
			y[ 9] += (z[17] + z[33]) * w;
			y[10] += (z[18] + z[34]) * w;
			y[11] += (z[19] + z[35]) * w;
			y[12] += (z[20] + z[36]) * w;
			y[13] += (z[21] + z[37]) * w;
			y[14] += (z[22] + z[38]) * w;
			y[15] += (z[23] + z[39]) * w;
			y[16] += (z[ 8] + z[40]) * w;
			y[17] += (z[ 9] + z[41]) * w;
			y[18] += (z[10] + z[42]) * w;
			y[19] += (z[11] + z[43]) * w;
			y[20] += (z[12] + z[44]) * w;
			y[21] += (z[13] + z[45]) * w;
			y[22] += (z[14] + z[46]) * w;
			y[23] += (z[15] + z[47]) * w;
			y[24] += ((M_SQRT2 * z[ 0]) + z[48]) * w;
			y[25] += ((M_SQRT2 * z[ 1]) + z[49]) * w;
			y[26] += ((M_SQRT2 * z[ 2]) + z[50]) * w;
			y[27] += ((M_SQRT2 * z[ 3]) + z[51]) * w;
			y[28] += ((M_SQRT2 * z[ 4]) + z[52]) * w;
			y[29] += ((M_SQRT2 * z[ 5]) + z[53]) * w;
			y[30] += ((M_SQRT2 * z[ 6]) + z[54]) * w;
			y[31] += ((M_SQRT2 * z[ 7]) + z[55]) * w;
			y[32] += (z[ 8] + z[56]) * w;
			y[33] += (z[ 9] + z[57]) * w;
			y[34] += (z[10] + z[58]) * w;
			y[35] += (z[11] + z[59]) * w;
			y[36] += (z[12] + z[60]) * w;
			y[37] += (z[13] + z[61]) * w;
			y[38] += (z[14] + z[62]) * w;
			y[39] += (z[15] + z[63]) * w;
			y[40] += (z[16]) * w;
			y[41] += (z[17]) * w;
			y[42] += (z[18]) * w;
			y[43] += (z[19]) * w;
			y[44] += (z[20]) * w;
			y[45] += (z[21]) * w;
			y[46] += (z[22]) * w;
			y[47] += (z[23]) * w;
			y[48] += (z[24] - z[56]) * w;
			y[49] += (z[25] - z[57]) * w;
			y[50] += (z[26] - z[58]) * w;
			y[51] += (z[27] - z[59]) * w;
			y[52] += (z[28] - z[60]) * w;
			y[53] += (z[29] - z[61]) * w;
			y[54] += (z[30] - z[62]) * w;
			y[55] += (z[31] - z[63]) * w;
			y[56] += (z[32] - z[48]) * w;
			y[57] += (z[33] - z[49]) * w;
			y[58] += (z[34] - z[50]) * w;
			y[59] += (z[35] - z[51]) * w;
			y[60] += (z[36] - z[52]) * w;
			y[61] += (z[37] - z[53]) * w;
			y[62] += (z[38] - z[54]) * w;
			y[63] += (z[39] - z[55]) * w;
			break;
		case 4:
			y[ 0] += ((M_SQRT2 * z[32])) * w;
			y[ 1] += ((M_SQRT2 * z[33])) * w;
			y[ 2] += ((M_SQRT2 * z[34])) * w;
			y[ 3] += ((M_SQRT2 * z[35])) * w;
			y[ 4] += ((M_SQRT2 * z[36])) * w;
			y[ 5] += ((M_SQRT2 * z[37])) * w;
			y[ 6] += ((M_SQRT2 * z[38])) * w;
			y[ 7] += ((M_SQRT2 * z[39])) * w;
			y[ 8] += (z[24] + z[40]) * w;
			y[ 9] += (z[25] + z[41]) * w;
			y[10] += (z[26] + z[42]) * w;
			y[11] += (z[27] + z[43]) * w;
			y[12] += (z[28] + z[44]) * w;
			y[13] += (z[29] + z[45]) * w;
			y[14] += (z[30] + z[46]) * w;
			y[15] += (z[31] + z[47]) * w;
			y[16] += (z[16] + z[48]) * w;
			y[17] += (z[17] + z[49]) * w;
			y[18] += (z[18] + z[50]) * w;
			y[19] += (z[19] + z[51]) * w;
			y[20] += (z[20] + z[52]) * w;
			y[21] += (z[21] + z[53]) * w;
			y[22] += (z[22] + z[54]) * w;
			y[23] += (z[23] + z[55]) * w;
			y[24] += (z[ 8] + z[56]) * w;
			y[25] += (z[ 9] + z[57]) * w;
			y[26] += (z[10] + z[58]) * w;
			y[27] += (z[11] + z[59]) * w;
			y[28] += (z[12] + z[60]) * w;
			y[29] += (z[13] + z[61]) * w;
			y[30] += (z[14] + z[62]) * w;
			y[31] += (z[15] + z[63]) * w;
			y[32] += ((M_SQRT2 * z[ 0])) * w;
			y[33] += ((M_SQRT2 * z[ 1])) * w;
			y[34] += ((M_SQRT2 * z[ 2])) * w;
			y[35] += ((M_SQRT2 * z[ 3])) * w;
			y[36] += ((M_SQRT2 * z[ 4])) * w;
			y[37] += ((M_SQRT2 * z[ 5])) * w;
			y[38] += ((M_SQRT2 * z[ 6])) * w;
			y[39] += ((M_SQRT2 * z[ 7])) * w;
			y[40] += (z[ 8] - z[56]) * w;
			y[41] += (z[ 9] - z[57]) * w;
			y[42] += (z[10] - z[58]) * w;
			y[43] += (z[11] - z[59]) * w;
			y[44] += (z[12] - z[60]) * w;
			y[45] += (z[13] - z[61]) * w;
			y[46] += (z[14] - z[62]) * w;
			y[47] += (z[15] - z[63]) * w;
			y[48] += (z[16] - z[48]) * w;
			y[49] += (z[17] - z[49]) * w;
			y[50] += (z[18] - z[50]) * w;
			y[51] += (z[19] - z[51]) * w;
			y[52] += (z[20] - z[52]) * w;
			y[53] += (z[21] - z[53]) * w;
			y[54] += (z[22] - z[54]) * w;
			y[55] += (z[23] - z[55]) * w;
			y[56] += (z[24] - z[40]) * w;
			y[57] += (z[25] - z[41]) * w;
			y[58] += (z[26] - z[42]) * w;
			y[59] += (z[27] - z[43]) * w;
			y[60] += (z[28] - z[44]) * w;
			y[61] += (z[29] - z[45]) * w;
			y[62] += (z[30] - z[46]) * w;
			y[63] += (z[31] - z[47]) * w;
			break;
		case 5:
			y[ 0] += ((M_SQRT2 * z[40])) * w;
			y[ 1] += ((M_SQRT2 * z[41])) * w;
			y[ 2] += ((M_SQRT2 * z[42])) * w;
			y[ 3] += ((M_SQRT2 * z[43])) * w;
			y[ 4] += ((M_SQRT2 * z[44])) * w;
			y[ 5] += ((M_SQRT2 * z[45])) * w;
			y[ 6] += ((M_SQRT2 * z[46])) * w;
			y[ 7] += ((M_SQRT2 * z[47])) * w;
			y[ 8] += (z[32] + z[48]) * w;
			y[ 9] += (z[33] + z[49]) * w;
			y[10] += (z[34] + z[50]) * w;
			y[11] += (z[35] + z[51]) * w;
			y[12] += (z[36] + z[52]) * w;
			y[13] += (z[37] + z[53]) * w;
			y[14] += (z[38] + z[54]) * w;
			y[15] += (z[39] + z[55]) * w;
			y[16] += (z[24] + z[56]) * w;
			y[17] += (z[25] + z[57]) * w;
			y[18] += (z[26] + z[58]) * w;
			y[19] += (z[27] + z[59]) * w;
			y[20] += (z[28] + z[60]) * w;
			y[21] += (z[29] + z[61]) * w;
			y[22] += (z[30] + z[62]) * w;
			y[23] += (z[31] + z[63]) * w;
			y[24] += (z[16]) * w;
			y[25] += (z[17]) * w;
			y[26] += (z[18]) * w;
			y[27] += (z[19]) * w;
			y[28] += (z[20]) * w;
			y[29] += (z[21]) * w;
			y[30] += (z[22]) * w;
			y[31] += (z[23]) * w;
			y[32] += (z[ 8] - z[56]) * w;
			y[33] += (z[ 9] - z[57]) * w;
			y[34] += (z[10] - z[58]) * w;
			y[35] += (z[11] - z[59]) * w;
			y[36] += (z[12] - z[60]) * w;
			y[37] += (z[13] - z[61]) * w;
			y[38] += (z[14] - z[62]) * w;
			y[39] += (z[15] - z[63]) * w;
			y[40] += ((M_SQRT2 * z[ 0]) - z[48]) * w;
			y[41] += ((M_SQRT2 * z[ 1]) - z[49]) * w;
			y[42] += ((M_SQRT2 * z[ 2]) - z[50]) * w;
			y[43] += ((M_SQRT2 * z[ 3]) - z[51]) * w;
			y[44] += ((M_SQRT2 * z[ 4]) - z[52]) * w;
			y[45] += ((M_SQRT2 * z[ 5]) - z[53]) * w;
			y[46] += ((M_SQRT2 * z[ 6]) - z[54]) * w;
			y[47] += ((M_SQRT2 * z[ 7]) - z[55]) * w;
			y[48] += (z[ 8] - z[40]) * w;
			y[49] += (z[ 9] - z[41]) * w;
			y[50] += (z[10] - z[42]) * w;
			y[51] += (z[11] - z[43]) * w;
			y[52] += (z[12] - z[44]) * w;
			y[53] += (z[13] - z[45]) * w;
			y[54] += (z[14] - z[46]) * w;
			y[55] += (z[15] - z[47]) * w;
			y[56] += (z[16] - z[32]) * w;
			y[57] += (z[17] - z[33]) * w;
			y[58] += (z[18] - z[34]) * w;
			y[59] += (z[19] - z[35]) * w;
			y[60] += (z[20] - z[36]) * w;
			y[61] += (z[21] - z[37]) * w;
			y[62] += (z[22] - z[38]) * w;
			y[63] += (z[23] - z[39]) * w;
			break;
		case 6:
			y[ 0] += ((M_SQRT2 * z[48])) * w;
			y[ 1] += ((M_SQRT2 * z[49])) * w;
			y[ 2] += ((M_SQRT2 * z[50])) * w;
			y[ 3] += ((M_SQRT2 * z[51])) * w;
			y[ 4] += ((M_SQRT2 * z[52])) * w;
			y[ 5] += ((M_SQRT2 * z[53])) * w;
			y[ 6] += ((M_SQRT2 * z[54])) * w;
			y[ 7] += ((M_SQRT2 * z[55])) * w;
			y[ 8] += (z[40] + z[56]) * w;
			y[ 9] += (z[41] + z[57]) * w;
			y[10] += (z[42] + z[58]) * w;
			y[11] += (z[43] + z[59]) * w;
			y[12] += (z[44] + z[60]) * w;
			y[13] += (z[45] + z[61]) * w;
			y[14] += (z[46] + z[62]) * w;
			y[15] += (z[47] + z[63]) * w;
			y[16] += (z[32]) * w;
			y[17] += (z[33]) * w;
			y[18] += (z[34]) * w;
			y[19] += (z[35]) * w;
			y[20] += (z[36]) * w;
			y[21] += (z[37]) * w;
			y[22] += (z[38]) * w;
			y[23] += (z[39]) * w;
			y[24] += (z[24] - z[56]) * w;
			y[25] += (z[25] - z[57]) * w;
			y[26] += (z[26] - z[58]) * w;
			y[27] += (z[27] - z[59]) * w;
			y[28] += (z[28] - z[60]) * w;
			y[29] += (z[29] - z[61]) * w;
			y[30] += (z[30] - z[62]) * w;
			y[31] += (z[31] - z[63]) * w;
			y[32] += (z[16] - z[48]) * w;
			y[33] += (z[17] - z[49]) * w;
			y[34] += (z[18] - z[50]) * w;
			y[35] += (z[19] - z[51]) * w;
			y[36] += (z[20] - z[52]) * w;
			y[37] += (z[21] - z[53]) * w;
			y[38] += (z[22] - z[54]) * w;
			y[39] += (z[23] - z[55]) * w;
			y[40] += (z[ 8] - z[40]) * w;
			y[41] += (z[ 9] - z[41]) * w;
			y[42] += (z[10] - z[42]) * w;
			y[43] += (z[11] - z[43]) * w;
			y[44] += (z[12] - z[44]) * w;
			y[45] += (z[13] - z[45]) * w;
			y[46] += (z[14] - z[46]) * w;
			y[47] += (z[15] - z[47]) * w;
			y[48] += ((M_SQRT2 * z[ 0]) - z[32]) * w;
			y[49] += ((M_SQRT2 * z[ 1]) - z[33]) * w;
			y[50] += ((M_SQRT2 * z[ 2]) - z[34]) * w;
			y[51] += ((M_SQRT2 * z[ 3]) - z[35]) * w;
			y[52] += ((M_SQRT2 * z[ 4]) - z[36]) * w;
			y[53] += ((M_SQRT2 * z[ 5]) - z[37]) * w;
			y[54] += ((M_SQRT2 * z[ 6]) - z[38]) * w;
			y[55] += ((M_SQRT2 * z[ 7]) - z[39]) * w;
			y[56] += (z[ 8] - z[24]) * w;
			y[57] += (z[ 9] - z[25]) * w;
			y[58] += (z[10] - z[26]) * w;
			y[59] += (z[11] - z[27]) * w;
			y[60] += (z[12] - z[28]) * w;
			y[61] += (z[13] - z[29]) * w;
			y[62] += (z[14] - z[30]) * w;
			y[63] += (z[15] - z[31]) * w;
			break;
		case 7:
			y[ 0] += ((M_SQRT2 * z[56])) * w;
			y[ 1] += ((M_SQRT2 * z[57])) * w;
			y[ 2] += ((M_SQRT2 * z[58])) * w;
			y[ 3] += ((M_SQRT2 * z[59])) * w;
			y[ 4] += ((M_SQRT2 * z[60])) * w;
			y[ 5] += ((M_SQRT2 * z[61])) * w;
			y[ 6] += ((M_SQRT2 * z[62])) * w;
			y[ 7] += ((M_SQRT2 * z[63])) * w;
			y[ 8] += (z[48]) * w;
			y[ 9] += (z[49]) * w;
			y[10] += (z[50]) * w;
			y[11] += (z[51]) * w;
			y[12] += (z[52]) * w;
			y[13] += (z[53]) * w;
			y[14] += (z[54]) * w;
			y[15] += (z[55]) * w;
			y[16] += (z[40] - z[56]) * w;
			y[17] += (z[41] - z[57]) * w;
			y[18] += (z[42] - z[58]) * w;
			y[19] += (z[43] - z[59]) * w;
			y[20] += (z[44] - z[60]) * w;
			y[21] += (z[45] - z[61]) * w;
			y[22] += (z[46] - z[62]) * w;
			y[23] += (z[47] - z[63]) * w;
			y[24] += (z[32] - z[48]) * w;
			y[25] += (z[33] - z[49]) * w;
			y[26] += (z[34] - z[50]) * w;
			y[27] += (z[35] - z[51]) * w;
			y[28] += (z[36] - z[52]) * w;
			y[29] += (z[37] - z[53]) * w;
			y[30] += (z[38] - z[54]) * w;
			y[31] += (z[39] - z[55]) * w;
			y[32] += (z[24] - z[40]) * w;
			y[33] += (z[25] - z[41]) * w;
			y[34] += (z[26] - z[42]) * w;
			y[35] += (z[27] - z[43]) * w;
			y[36] += (z[28] - z[44]) * w;
			y[37] += (z[29] - z[45]) * w;
			y[38] += (z[30] - z[46]) * w;
			y[39] += (z[31] - z[47]) * w;
			y[40] += (z[16] - z[32]) * w;
			y[41] += (z[17] - z[33]) * w;
			y[42] += (z[18] - z[34]) * w;
			y[43] += (z[19] - z[35]) * w;
			y[44] += (z[20] - z[36]) * w;
			y[45] += (z[21] - z[37]) * w;
			y[46] += (z[22] - z[38]) * w;
			y[47] += (z[23] - z[39]) * w;
			y[48] += (z[ 8] - z[24]) * w;
			y[49] += (z[ 9] - z[25]) * w;
			y[50] += (z[10] - z[26]) * w;
			y[51] += (z[11] - z[27]) * w;
			y[52] += (z[12] - z[28]) * w;
			y[53] += (z[13] - z[29]) * w;
			y[54] += (z[14] - z[30]) * w;
			y[55] += (z[15] - z[31]) * w;
			y[56] += ((M_SQRT2 * z[ 0]) - z[16]) * w;
			y[57] += ((M_SQRT2 * z[ 1]) - z[17]) * w;
			y[58] += ((M_SQRT2 * z[ 2]) - z[18]) * w;
			y[59] += ((M_SQRT2 * z[ 3]) - z[19]) * w;
			y[60] += ((M_SQRT2 * z[ 4]) - z[20]) * w;
			y[61] += ((M_SQRT2 * z[ 5]) - z[21]) * w;
			y[62] += ((M_SQRT2 * z[ 6]) - z[22]) * w;
			y[63] += ((M_SQRT2 * z[ 7]) - z[23]) * w;
			break;
	}

	return;
}