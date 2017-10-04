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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <getopt.h>

#include "libmodjpeg.h"

static struct option longopts[] = {
	{ "input",	required_argument,	NULL,		'i' },
	{ "output",	required_argument,	NULL,		'o' },
	{ "dropon",	required_argument,	NULL,		'd' },
	{ "position",	required_argument,	NULL,		'p' },
	{ "offset",	required_argument,	NULL,		'm' },
	{ "luminance",	required_argument,	NULL,		'y' },
	{ "tintblue",	required_argument,	NULL,		'b' },
	{ "tintred",	required_argument,	NULL,		'r' },
	{ "pixelate",	no_argument,		NULL,		'x' },
	{ "grayscale",	no_argument,		NULL,		'g' },
	{ "help",	no_argument,		NULL,		'h' },
	{ NULL,		0,			NULL,		0 }
};

void help(void);

int main(int argc, char *argv[]) {
	int c, opterr, t, position = MJ_ALIGN_TOP | MJ_ALIGN_LEFT, offset_x = 0, offset_y = 0;
	char *str;
	mj_jpeg_t *m = NULL;
	mj_dropon_t *d = NULL;

	opterr = 1;

	while((c = getopt_long(argc, argv, ":i: :o: :d: :p: :m: :y: :b: :r: xgh", longopts, NULL)) != -1) {
		switch(c) {
			case 'i':
				if(m != NULL) {
					mj_destroy_jpeg(m);
					m = NULL;
				}

				m = mj_read_jpeg_from_file(optarg);
				if(m == NULL) {
					fprintf(stderr, "can't read image from '%s'\n", optarg);
					exit(1);
				}

				break;
			case 'o':
				if(m == NULL) {
					fprintf(stderr, "can't write image without loading an image first (--input)\n");
					exit(1);
				}

				if(mj_write_jpeg_to_file(m, optarg) != 0) {
					fprintf(stderr, "can't write image to '%s'\n", optarg);
					exit(1);
				}

				break;
			case 'd':
				if(m == NULL) {
					fprintf(stderr, "can't apply a dropon without loading an image first (--input)\n");
					exit(1);
				}

				if(d != NULL) {
					mj_destroy_dropon(d);
					d = NULL;
				}

				str = strchr(optarg, ',');
				if(str != NULL) {
					*str = '\0';
					d = mj_read_dropon_from_jpeg(optarg, str + 1, MJ_BLEND_FULL);
				}
				else {
					d = mj_read_dropon_from_jpeg(optarg, NULL, MJ_BLEND_FULL);
				}

				if(d == NULL) {
					fprintf(stderr, "can't read dropon from '%s'\n", optarg);
					exit(1);
				}

				mj_compose(m, d, position, offset_x, offset_y);

				break;
			case 'p':
				if(strlen(optarg) != 2) {
					fprintf(stderr, "invalid position, use --help for more details\n");
					break;
				}

				position = 0;
				if(optarg[0] == 't') {
					position |= MJ_ALIGN_TOP;
				}
				else if(optarg[0] == 'b') {
					position |= MJ_ALIGN_BOTTOM;
				}
				else if(optarg[0] == 'c') {
					position |= MJ_ALIGN_CENTER;
				}

				if(optarg[1] == 'l') {
					position |= MJ_ALIGN_LEFT;
				}
				else if(optarg[1] == 'r') {
					position |= MJ_ALIGN_RIGHT;
				}
				else if(optarg[1] == 'c') {
					position |= MJ_ALIGN_CENTER;
				}

				break;
			case 'm':
				offset_x = (int)strtol(optarg, NULL, 10);
				str = strchr(optarg, ',');
				if(str != NULL) {
					offset_y = (int)strtol(++str, NULL, 10);
				}
				break;
			case 'y':
				if(m == NULL) {
					fprintf(stderr, "can't apply effect without loading an image first (--input)\n");
					exit(1);
				}

				t = (int)strtol(optarg, NULL, 10);
				mj_effect_luminance(m, t);
				break;
			case 'b':
				if(m == NULL) {
					fprintf(stderr, "can't apply effect without loading an image first (--input)\n");
					exit(1);
				}

				t = (int)strtol(optarg, NULL, 10);
				mj_effect_tint(m, t, 0);
				break;
			case 'r':
				if(m == NULL) {
					fprintf(stderr, "can't apply effect without loading an image first (--input)\n");
					exit(1);
				}

				t = (int)strtol(optarg, NULL, 10);
				mj_effect_tint(m, 0, t);
				break;
			case 'x':
				if(m == NULL) {
					fprintf(stderr, "can't apply effect without loading an image first (--input)\n");
					exit(1);
				}

				mj_effect_pixelate(m);
				break;
			case 'g':
				if(m == NULL) {
					fprintf(stderr, "can't apply effect without loading an image first (--input)\n");
					exit(1);
				}
				
				mj_effect_grayscale(m);
				break;
			case 'h':
				help();
				exit(0);
			case ':':
				fprintf(stderr, "argument missing, use --help for more details\n");
				break;
			case '?':
			default:
				fprintf(stderr, "unknown option, use --help for more details\n");
				break;
		}
	}

	if(m != NULL) {
		mj_destroy_jpeg(m);
	}

	if(d != NULL) {
		mj_destroy_dropon(d);
	}

	return 0;
}

void help(void) {
	fprintf(stderr, "modjpeg (c) 2006+ Ingo Oppermann\n\n");
	fprintf(stderr, "Optionen:\n");

	fprintf(stderr, "\t--input, -i file\n");
	fprintf(stderr, "\t\tName des zu modifizierenden Bildes. Wird diese Option weggelassen\n");
	fprintf(stderr, "\t\toder ist file gleich '-', wird stdin als Input genommen. Das Bild muss\n");
	fprintf(stderr, "\t\tim JPEG-Format sein.\n");
	fprintf(stderr, "\n");

	fprintf(stderr, "\t--ouput, -o file\n");
	fprintf(stderr, "\t\tName des resultierenden Bildes. Wird diese Option weggelassen\n");
	fprintf(stderr, "\t\toder ist file gleich '-', wird stdout als Output genommen.\n");
	fprintf(stderr, "\n");

	fprintf(stderr, "\tBei den folgenden Optionen ist die Reihenfolge wichtig, in der sie stehen.\n\n");

	fprintf(stderr, "\t--dropon, -d file[,mask]\n");
	fprintf(stderr, "\t\tName des Bildes, das als Logo verwendet werden soll. Die Maske ist optional.\n");
	fprintf(stderr, "\t\tLogo und Maske muessen im JPEG-Format sein.\n");
	fprintf(stderr, "\n");

	fprintf(stderr, "\t--position, -p [t|b][c][l|r]\n");
	fprintf(stderr, "\t\tDie Position des dropon. t = top, b = bottom, l = left, r = right, c = center.\n");
	fprintf(stderr, "\n");

	fprintf(stderr, "\t--luminance, -y value\n");
	fprintf(stderr, "\t\tVeraendert die Helligkeit des Bildes entsprechend des Wertes in value.\n");
	fprintf(stderr, "\n");

	fprintf(stderr, "\t--tintblue, -b value\n");
	fprintf(stderr, "\t\tEin positiver Wert faerbt das Bild blau, ein negativer Wert faerbt\n");
	fprintf(stderr, "\t\tdas Bild gelb.\n");
	fprintf(stderr, "\n");

	fprintf(stderr, "\t--tintred, -r value\n");
	fprintf(stderr, "\t\tEin positiver Wert faerbt das Bild rot, ein negativer Wert faerbt\n");
	fprintf(stderr, "\t\tdas Bild gruen.\n");
	fprintf(stderr, "\n");

	fprintf(stderr, "\t--pixelate, -x\n");
	fprintf(stderr, "\t\tVerpixelt das Bild in 8x8-Bloecken.\n");
	fprintf(stderr, "\n");

	fprintf(stderr, "\t--grayscale, -g\n");
	fprintf(stderr, "\t\tReduziert das Bild zu Graustufen.\n");
	fprintf(stderr, "\n");

	fprintf(stderr, "Beispiele:\n");
	fprintf(stderr, "\n");

	fprintf(stderr, "\tEin Logo in der rechten oberen Ecke platzieren:\n");
	fprintf(stderr, "\t\tmodjpeg --input in.jpg --position tr --dropon logo.jpg --output out.jpg\n");
	fprintf(stderr, "\n");

	fprintf(stderr, "\tEin Logo in der rechten oberen Ecke platzieren und nachher das Bild\n");
	fprintf(stderr, "\tverpixeln:\n");
	fprintf(stderr, "\t\tmodjpeg --input in.jpg --position tr --dropon logo.jpg --pixelate --output out.jpg\n");
	fprintf(stderr, "\n");

	fprintf(stderr, "\tErst das Bild verpixeln, dann ein Logo in der rechten oberen Ecke\n");
	fprintf(stderr, "\tplatzieren:\n");
	fprintf(stderr, "\t\tmodjpeg --input in.jpg --pixelate --position tr --dropon logo.jpg --output out.jpg\n");
	fprintf(stderr, "\n");

	fprintf(stderr, "\n");
	return;
}

