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

#include <stdlib.h>
#include <string.h>

#ifdef WITH_LIBPNG
#    include <png.h>
#endif

#include "dropon.h"
#include "image.h"
#include "libmodjpeg.h"

int mj_read_dropon_from_file(mj_dropon_t *d, const char *filename, const char *maskfilename, short blend) {
    if(d == NULL) {
        return MJ_ERR_NULL_DATA;
    }

    int            rv;
    unsigned char *memory = NULL, *maskmemory = NULL;
    size_t         len = 0, masklen = 0;

    rv = mj_read_file(&memory, &len, filename);
    if(rv != MJ_OK) {
        return rv;
    }

    if(maskfilename != NULL) {
        rv = mj_read_file(&maskmemory, &masklen, maskfilename);
        if(rv != MJ_OK) {
            free(memory);
            return rv;
        }
    }

    rv = mj_read_dropon_from_memory(d, memory, len, maskmemory, masklen, blend);

    free(memory);
    if(maskmemory != NULL) {
        free(maskmemory);
    }

    return rv;
}

int mj_read_dropon_from_memory(mj_dropon_t *d, const unsigned char *memory, size_t len, const unsigned char *maskmemory, size_t masklen, short blend) {
    if(d == NULL || memory == NULL || len < 8) {
        return MJ_ERR_NULL_DATA;
    }

    int rv = MJ_OK;

    // Test for JPEG
    if(
        memory[0] == 0xff &&
        memory[1] == 0xd8 &&
        memory[2] == 0xff) {
        rv = mj_read_dropon_from_jpeg_memory(d, memory, len, maskmemory, masklen, blend);
    }
#ifdef WITH_LIBPNG
    // Test for PNG
    else if(
        memory[0] == 0x89 &&
        memory[1] == 'P' &&
        memory[2] == 'N' &&
        memory[3] == 'G' &&
        memory[4] == 0x0d &&
        memory[5] == 0x0a &&
        memory[6] == 0x1a &&
        memory[7] == 0x0a) {
        rv = mj_read_dropon_from_png_memory(d, memory, len);
    }
#endif
    else {
        return MJ_ERR_UNSUPPORTED_FILETYPE;
    }

    return rv;
}

int mj_read_dropon_from_jpeg_memory(mj_dropon_t *d, const unsigned char *memory, size_t len, const unsigned char *maskmemory, size_t masklen, short blend) {
    unsigned char *image_buffer = NULL, *alpha_buffer = NULL, *buffer = NULL;
    int            colorspace, rv;
    int            image_width = 0, alpha_width = 0;
    int            image_height = 0, alpha_height = 0;

    rv = mj_decode_jpeg_memory_to_raw(&image_buffer, &image_width, &image_height, MJ_COLORSPACE_RGB, memory, len);
    if(rv != MJ_OK) {
        return rv;
    }

    if(maskmemory != NULL && masklen != 0) {
        rv = mj_decode_jpeg_memory_to_raw(&alpha_buffer, &alpha_width, &alpha_height, MJ_COLORSPACE_GRAYSCALE, maskmemory, masklen);
        if(rv != MJ_OK) {
            free(image_buffer);
            return rv;
        }

        if(image_width != alpha_width || image_height != alpha_height) {
            free(image_buffer);
            free(alpha_buffer);

            return MJ_ERR_DROPON_DIMENSIONS;
        }

        buffer = (unsigned char *)calloc(4 * image_width * image_height, sizeof(unsigned char));
        if(buffer == NULL) {
            free(image_buffer);
            free(alpha_buffer);

            return MJ_ERR_MEMORY;
        }

        size_t         v;
        unsigned char *t = buffer, *p = image_buffer, *q = alpha_buffer;

        for(v = 0; v < ((size_t)image_width * (size_t)image_height); v++) {
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

    rv = mj_read_dropon_from_raw(d, buffer, colorspace, image_width, image_height, blend);

    free(image_buffer);

    if(alpha_buffer != NULL) {
        free(alpha_buffer);
        free(buffer);
    }

    return rv;
}

#ifdef WITH_LIBPNG
int mj_read_dropon_from_png_memory(mj_dropon_t *d, const unsigned char *memory, size_t len) {
    png_image image;

    memset(&image, 0, sizeof(png_image));
    image.version = PNG_IMAGE_VERSION;

    if(png_image_begin_read_from_memory(&image, memory, len) == 0) {
        return MJ_ERR_FILEIO;
    }

    if(image.width >= (2 << 16) || image.height >= (2 << 16)) {
        png_image_free(&image);
        return MJ_ERR_DROPON_DIMENSIONS;
    }

    png_bytep buffer;
    image.format = PNG_FORMAT_RGBA;

    buffer = malloc(PNG_IMAGE_SIZE(image));
    if(buffer == NULL) {
        return MJ_ERR_MEMORY;
    }

    if(png_image_finish_read(&image, NULL, buffer, 0, NULL) == 0) {
        free(buffer);
        return MJ_ERR_FILEIO;
    }

    int rv;
    rv = mj_read_dropon_from_raw(d, buffer, MJ_COLORSPACE_RGBA, image.width, image.height, MJ_BLEND_NONUNIFORM);

    free(buffer);

    png_image_free(&image);

    return rv;
}
#endif

int mj_read_dropon_from_raw(mj_dropon_t *d, const unsigned char *rawdata, unsigned int colorspace, int width, int height, short blend) {
    if(d == NULL) {
        return MJ_ERR_NULL_DATA;
    }

    mj_free_dropon(d);

    if(rawdata == NULL) {
        return MJ_ERR_NULL_DATA;
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
            return MJ_ERR_UNSUPPORTED_COLORSPACE;
    }

    d->width = width;
    d->height = height;
    d->blend = blend;

    // image and alpha are store with 3 components. this makes it
    // easier to handle later for compiling the dropon.
    size_t nsamples = 3 * width * height;

    d->image = (unsigned char *)calloc(nsamples, sizeof(unsigned char));
    if(d->image == NULL) {
        mj_free_dropon(d);
        return MJ_ERR_MEMORY;
    }

    // the alpha channel is also stored with 3 component
    d->alpha = (unsigned char *)calloc(nsamples, sizeof(unsigned char));
    if(d->alpha == NULL) {
        mj_free_dropon(d);
        return MJ_ERR_MEMORY;
    }

    const unsigned char *p = rawdata;
    unsigned char *      pimage = d->image;
    unsigned char *      palpha = d->alpha;

    size_t v = 0;

    if(colorspace == MJ_COLORSPACE_RGBA || colorspace == MJ_COLORSPACE_YCCA) {
        for(v = 0; v < ((size_t)width * (size_t)height); v++) {
            *pimage++ = *p++;
            *pimage++ = *p++;
            *pimage++ = *p++;

            *palpha++ = *p;
            *palpha++ = *p;
            *palpha++ = *p++;
        }

        if(colorspace == MJ_COLORSPACE_RGBA) {
            d->colorspace = MJ_COLORSPACE_RGB;
        }
        else {
            d->colorspace = MJ_COLORSPACE_YCC;
        }

        d->blend = MJ_BLEND_NONUNIFORM;
    }
    else if(colorspace == MJ_COLORSPACE_RGB || colorspace == MJ_COLORSPACE_YCC) {
        for(v = 0; v < ((size_t)width * (size_t)height); v++) {
            *pimage++ = *p++;
            *pimage++ = *p++;
            *pimage++ = *p++;

            *palpha++ = (char)d->blend;
            *palpha++ = (char)d->blend;
            *palpha++ = (char)d->blend;
        }

        d->colorspace = colorspace;
    }
    else if(colorspace == MJ_COLORSPACE_GRAYSCALEA) {
        for(v = 0; v < ((size_t)width * (size_t)height); v++) {
            *pimage++ = *p;
            *pimage++ = *p;
            *pimage++ = *p++;

            *palpha++ = *p;
            *palpha++ = *p;
            *palpha++ = *p++;
        }

        d->colorspace = MJ_COLORSPACE_GRAYSCALE;

        d->blend = MJ_BLEND_NONUNIFORM;
    }
    else {
        for(v = 0; v < ((size_t)width * (size_t)height); v++) {
            *pimage++ = *p;
            *pimage++ = *p;
            *pimage++ = *p++;

            *palpha++ = (char)d->blend;
            *palpha++ = (char)d->blend;
            *palpha++ = (char)d->blend;
        }

        d->colorspace = MJ_COLORSPACE_GRAYSCALE;
    }

    return MJ_OK;
}

int mj_compile_dropon(mj_compileddropon_t *cd, mj_dropon_t *d, J_COLOR_SPACE colorspace, mj_sampling_t *sampling, int blockoffset_x, int blockoffset_y, int crop_x, int crop_y, int crop_w, int crop_h) {
    if(cd == NULL || d == NULL) {
        return MJ_ERR_NULL_DATA;
    }

    // crop and or extend the dropon. the dropon needs to cover whole blocks.

    // after that, encode it to a jpeg with the same colorspace and sampling as the image.
    // this gives us the the dropon in frequency space.

    // same for the mask. the mask is required if we extend the dropon such that
    // the extended area doesn't cover the image.

    // crop/extend the dropon

    int width = crop_w + blockoffset_x;
    int padding = width % sampling->h_factor;
    if(padding != 0) {
        width += sampling->h_factor - padding;
    }

    int height = crop_h + blockoffset_y;
    padding = height % sampling->v_factor;
    if(padding != 0) {
        height += sampling->v_factor - padding;
    }

    unsigned char *data = (unsigned char *)calloc(3 * width * height, sizeof(unsigned char));
    if(data == NULL) {
        return MJ_ERR_MEMORY;
    }

    int            i, j;
    unsigned char *p, *q;

    for(i = crop_y; i < (crop_y + crop_h); i++) {
        p = &data[(i - crop_y + blockoffset_y) * width * 3 + (blockoffset_x * 3)];
        q = &d->image[i * d->width * 3 + (crop_x * 3)];

        for(j = crop_x; j < (crop_x + crop_w); j++) {
            *p++ = *q++;
            *p++ = *q++;
            *p++ = *q++;
        }
    }

    int            rv;
    unsigned char *buffer = NULL;
    size_t         len = 0;

    // encode the dropon to JPEG
    rv = mj_encode_raw_to_jpeg_memory(&buffer, &len, data, d->colorspace, colorspace, sampling, width, height);
    if(rv != MJ_OK) {
        free(data);
        return rv;
    }

    // read the coefficients from the encoded dropon
    rv = mj_read_droponimage_from_memory(cd, buffer, len);
    free(buffer);

    if(rv != MJ_OK) {
        free(data);
        return rv;
    }

    for(i = crop_y; i < (crop_y + crop_h); i++) {
        p = &data[(i - crop_y + blockoffset_y) * width * 3 + (blockoffset_x * 3)];
        q = &d->alpha[i * d->width * 3 + (crop_x * 3)];

        for(j = crop_x; j < (crop_x + crop_w); j++) {
            *p++ = *q++;
            *p++ = *q++;
            *p++ = *q++;
        }
    }

    // encode the mask to JPEG. all components of the mask are the same. depending on the targeted colorspace,
    // the data of the mask needs to be interpreted differently
    // for the target colorspaces, we support only GRAYSCALE, RGB, and YCC
    // the JPEG library supports only these internal color transformations
    //    RGB => RGB
    //    RGB => YCC
    //    RGB => GRAYSCALE
    //    YCC => YCC
    //    YCC => GRAYSCALE
    int alpha_colorspace = MJ_COLORSPACE_YCC;
    if(colorspace == JCS_RGB) {
        alpha_colorspace = MJ_COLORSPACE_RGB;
    }
    rv = mj_encode_raw_to_jpeg_memory(&buffer, &len, data, alpha_colorspace, colorspace, sampling, width, height);
    if(rv != MJ_OK) {
        free(data);
        return rv;
    }

    // read the coefficients from the encoded dropon mask
    rv = mj_read_droponalpha_from_memory(cd, buffer, len);

    free(buffer);
    free(data);

    return rv;
}

int mj_read_droponimage_from_memory(mj_compileddropon_t *cd, const unsigned char *memory, size_t len) {
    if(cd == NULL) {
        return MJ_ERR_NULL_DATA;
    }

    mj_jpeg_t m;
    int       rv;

    mj_init_jpeg(&m);

    rv = mj_read_jpeg_from_memory(&m, memory, len, 0);
    if(rv != MJ_OK) {
        return rv;
    }

    int                  c, k, l, i;
    jpeg_component_info *component;
    mj_component_t *     comp;
    mj_block_t *         b;
    JBLOCKARRAY          blocks;
    JCOEFPTR             coefs;

    cd->image_ncomponents = m.cinfo.num_components;
    cd->image_colorspace = m.cinfo.jpeg_color_space;
    cd->image = (mj_component_t *)calloc(cd->image_ncomponents, sizeof(mj_component_t));

    for(c = 0; c < m.cinfo.num_components; c++) {
        component = &m.cinfo.comp_info[c];
        comp = &cd->image[c];

        comp->h_samp_factor = component->h_samp_factor;
        comp->v_samp_factor = component->v_samp_factor;

        comp->width_in_blocks = component->width_in_blocks;
        comp->height_in_blocks = component->height_in_blocks;

        comp->nblocks = comp->width_in_blocks * comp->height_in_blocks;
        comp->blocks = (mj_block_t **)calloc(comp->nblocks, sizeof(mj_block_t *));

        for(l = 0; l < comp->height_in_blocks; l++) {
            blocks = (*m.cinfo.mem->access_virt_barray)((j_common_ptr)&m.cinfo, m.coef[c], l, 1, TRUE);

            for(k = 0; k < comp->width_in_blocks; k++) {
                b = (mj_block_t *)calloc(64, sizeof(mj_block_t));
                coefs = blocks[0][k];

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

                comp->blocks[comp->width_in_blocks * l + k] = b;
            }
        }
    }

    mj_free_jpeg(&m);

    return MJ_OK;
}

int mj_read_droponalpha_from_memory(mj_compileddropon_t *cd, const unsigned char *memory, size_t len) {
    if(cd == NULL) {
        return MJ_ERR_NULL_DATA;
    }

    mj_jpeg_t m;
    int       rv;

    mj_init_jpeg(&m);

    rv = mj_read_jpeg_from_memory(&m, memory, len, 0);
    if(rv != MJ_OK) {
        return rv;
    }

    int                  c, k, l, i;
    jpeg_component_info *component;
    mj_component_t *     comp;
    mj_block_t *         b;
    JBLOCKARRAY          blocks;
    JCOEFPTR             coefs;

    cd->alpha_ncomponents = m.cinfo.num_components;
    cd->alpha = (mj_component_t *)calloc(cd->alpha_ncomponents, sizeof(mj_component_t));

    for(c = 0; c < m.cinfo.num_components; c++) {
        component = &m.cinfo.comp_info[c];
        comp = &cd->alpha[c];

        comp->h_samp_factor = component->h_samp_factor;
        comp->v_samp_factor = component->v_samp_factor;

        comp->width_in_blocks = component->width_in_blocks;
        comp->height_in_blocks = component->height_in_blocks;

        comp->nblocks = comp->width_in_blocks * comp->height_in_blocks;
        comp->blocks = (mj_block_t **)calloc(comp->nblocks, sizeof(mj_block_t *));

        for(l = 0; l < comp->height_in_blocks; l++) {
            blocks = (*m.cinfo.mem->access_virt_barray)((j_common_ptr)&m.cinfo, m.coef[c], l, 1, TRUE);

            for(k = 0; k < comp->width_in_blocks; k++) {
                b = (mj_block_t *)calloc(64, sizeof(mj_block_t));
                coefs = blocks[0][k];

                coefs[0] += 1024;

                // w'(j, i) = w(j, i) * 1/255 * c(i) * c(j) * 1/4
                // the factor 1/4 comes from V(i) and V(j)
                // => 1/255 * 1/4 = 1/1020

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

                comp->blocks[comp->width_in_blocks * l + k] = b;
            }
        }
    }

    mj_free_jpeg(&m);

    return MJ_OK;
}

void mj_init_dropon(mj_dropon_t *d) {
    if(d == NULL) {
        return;
    }

    memset(d, 0, sizeof(mj_dropon_t));

    return;
}

void mj_free_dropon(mj_dropon_t *d) {
    if(d == NULL) {
        return;
    }

    if(d->image != NULL) {
        free(d->image);
    }

    if(d->alpha != NULL) {
        free(d->alpha);
    }

    mj_init_dropon(d);

    return;
}

void mj_free_compileddropon(mj_compileddropon_t *cd) {
    if(cd == NULL) {
        return;
    }

    int i;

    if(cd->image != NULL) {
        for(i = 0; i < cd->image_ncomponents; i++) {
            mj_free_component(&cd->image[i]);
        }
        free(cd->image);
        cd->image = NULL;
    }

    if(cd->alpha != NULL) {
        for(i = 0; i < cd->alpha_ncomponents; i++) {
            mj_free_component(&cd->alpha[i]);
        }
        free(cd->alpha);
        cd->alpha = NULL;
    }

    return;
}

void mj_free_component(mj_component_t *c) {
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
