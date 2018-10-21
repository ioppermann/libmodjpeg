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

#include "libmodjpeg.h"
#include "effect.h"
#include "jpeg.h"

int mj_effect_grayscale(mj_jpeg_t *m) {
    int i, c;
    JDIMENSION k, l;
    jpeg_component_info *component;
    JBLOCKARRAY blocks;
    JCOEFPTR coefs;

    if(m == NULL || m->coef == NULL) {
        return MJ_ERR_NULL_DATA;
    }

    if(m->cinfo.jpeg_color_space != JCS_YCbCr) {
        return MJ_OK;
    }

    /* set all color components to 0 */
    for(c = 1; c < m->cinfo.num_components; c++) {
        component = &m->cinfo.comp_info[c];

        for(l = 0; l < component->height_in_blocks; l++) {
            blocks = (*m->cinfo.mem->access_virt_barray)((j_common_ptr)&m->cinfo, m->coef[c], l, 1, TRUE);

            for(k = 0; k < component->width_in_blocks; k++) {
                coefs = blocks[0][k];

                for(i = 0; i < DCTSIZE2; i += 8) {
                    coefs[i + 0] = 0;
                    coefs[i + 1] = 0;
                    coefs[i + 2] = 0;
                    coefs[i + 3] = 0;
                    coefs[i + 4] = 0;
                    coefs[i + 5] = 0;
                    coefs[i + 6] = 0;
                    coefs[i + 7] = 0;
                }
            }
        }
    }

    return MJ_OK;
}

int mj_effect_pixelate(mj_jpeg_t *m) {
    int i, c;
    JDIMENSION k, l;
    jpeg_component_info *component;
    JBLOCKARRAY blocks;
    JCOEFPTR coefs;

    if(m == NULL || m->coef == NULL) {
        return MJ_ERR_NULL_DATA;
    }

    /* Set all the AC coefficients to 0 */
    for(c = 0; c < m->cinfo.num_components; c++) {
        component = &m->cinfo.comp_info[c];

        for(l = 0; l < component->height_in_blocks; l++) {
            blocks = (*m->cinfo.mem->access_virt_barray)((j_common_ptr)&m->cinfo, m->coef[c], l, 1, TRUE);

            for(k = 0; k < component->width_in_blocks; k++) {
                coefs = blocks[0][k];

                coefs[1] = 0;
                coefs[2] = 0;
                coefs[3] = 0;
                coefs[4] = 0;
                coefs[5] = 0;
                coefs[6] = 0;
                coefs[7] = 0;

                for(i = 8; i < DCTSIZE2; i += 8) {
                    coefs[i + 0] = 0;
                    coefs[i + 1] = 0;
                    coefs[i + 2] = 0;
                    coefs[i + 3] = 0;
                    coefs[i + 4] = 0;
                    coefs[i + 5] = 0;
                    coefs[i + 6] = 0;
                    coefs[i + 7] = 0;
                }
            }
        }
    }

    return MJ_OK;
}

int mj_effect_tint(mj_jpeg_t *m, int cb_value, int cr_value) {
    JDIMENSION k, l;
    jpeg_component_info *component;
    JBLOCKARRAY blocks;
    JCOEFPTR coefs;

    if(m == NULL || m->coef == NULL) {
        return MJ_ERR_NULL_DATA;
    }

    if(m->cinfo.jpeg_color_space != JCS_YCbCr) {
        return MJ_OK;
    }

    if(cb_value == 0 && cr_value == 0) {
        return MJ_OK;
    }

    if(cb_value != 0) {
        component = &m->cinfo.comp_info[1];

        for(l = 0; l < component->height_in_blocks; l++) {
            blocks = (*m->cinfo.mem->access_virt_barray)((j_common_ptr)&m->cinfo, m->coef[1], l, 1, TRUE);

            for(k = 0; k < component->width_in_blocks; k++) {
                coefs = blocks[0][k];

                coefs[0] *= component->quant_table->quantval[0];
                coefs[0] += cb_value;

                if(coefs[0] > 2047) {
                    coefs[0] = 2047;
                }
                else if(coefs[0] < -2047) {
                    coefs[0] = -2047;
                }

                coefs[0] /= component->quant_table->quantval[0];
            }
        }
    }

    if(cr_value != 0) {
        component = &m->cinfo.comp_info[2];

        for(l = 0; l < component->height_in_blocks; l++) {
            blocks = (*m->cinfo.mem->access_virt_barray)((j_common_ptr)&m->cinfo, m->coef[2], l, 1, TRUE);

            for(k = 0; k < component->width_in_blocks; k++) {
                coefs = blocks[0][k];

                coefs[0] *= component->quant_table->quantval[0];
                coefs[0] += cr_value;

                if(coefs[0] > 2047) {
                    coefs[0] = 2047;
                }
                else if(coefs[0] < -2047) {
                    coefs[0] = -2047;
                }

                coefs[0] /= component->quant_table->quantval[0];
            }
        }
    }

    return MJ_OK;
}

int mj_effect_luminance(mj_jpeg_t *m, int value) {
    JDIMENSION k, l;
    jpeg_component_info *component;
    JBLOCKARRAY blocks;
    JCOEFPTR coefs;

    if(m == NULL || m->coef == NULL) {
        return MJ_ERR_NULL_DATA;
    }

    if(m->cinfo.jpeg_color_space != JCS_YCbCr) {
        return MJ_OK;
    }

    component = &m->cinfo.comp_info[0];

    for(l = 0; l < component->height_in_blocks; l++) {
        blocks = (*m->cinfo.mem->access_virt_barray)((j_common_ptr)&m->cinfo, m->coef[0], l, 1, TRUE);

        for(k = 0; k < component->width_in_blocks; k++) {
            coefs = blocks[0][k];

            coefs[0] *= component->quant_table->quantval[0];
            coefs[0] += value;

            if(coefs[0] > 2047) {
                coefs[0] = 2047;
            }
            else if(coefs[0] < -2047) {
                coefs[0] = -2047;
            }

            coefs[0] /= component->quant_table->quantval[0];
        }
    }

    return MJ_OK;
}
