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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <getopt.h>

#include "libmodjpeg.h"

static struct option longopts[] = {
	{ "input",       required_argument, NULL, 'i' },
	{ "output",      required_argument, NULL, 'o' },
	{ "dropon",      required_argument, NULL, 'd' },
	{ "position",    required_argument, NULL, 'p' },
	{ "offset",      required_argument, NULL, 'm' },
	{ "luminance",   required_argument, NULL, 'y' },
	{ "tintblue",    required_argument, NULL, 'b' },
	{ "tintred",     required_argument, NULL, 'r' },
	{ "pixelate",    no_argument,       NULL, 'x' },
	{ "grayscale",   no_argument,       NULL, 'g' },
	{ "progressive", no_argument,       NULL, 'P' },
	{ "optimize",    no_argument,       NULL, 'O' },
	{ "help",        no_argument,       NULL, 'h' },
	{ NULL,          0,                 NULL,  0  }
};

void help(void);

int main(int argc, char *argv[]) {
	int c, opterr, t, position = MJ_ALIGN_TOP | MJ_ALIGN_LEFT, offset_x = 0, offset_y = 0, options = 0;
	char *str;
	mj_jpeg_t *m = NULL;
	mj_dropon_t *d = NULL;

	opterr = 1;

	while((c = getopt_long(argc, argv, ":i: :o: :d: :p: :m: :y: :b: :r: xgPOh", longopts, NULL)) != -1) {
		switch(c) {
			case 'i':
				if(m != NULL) {
					mj_destroy_jpeg(m);
					m = NULL;
				}

				m = mj_read_jpeg_from_file(optarg);
				if(m == NULL) {
					fprintf(stderr, "Can't read image from '%s'\n", optarg);
					exit(1);
				}

				break;
			case 'o':
				if(m == NULL) {
					fprintf(stderr, "Can't write image without loading an image first (--input)\n");
					exit(1);
				}

				if(mj_write_jpeg_to_file(m, optarg, options) != MJ_ERR) {
					fprintf(stderr, "Can't write image to '%s'\n", optarg);
					exit(1);
				}

				break;
			case 'd':
				if(m == NULL) {
					fprintf(stderr, "Can't apply a dropon without loading an image first (--input)\n");
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
					fprintf(stderr, "Can't read dropon from '%s'\n", optarg);
					exit(1);
				}

				mj_compose(m, d, position, offset_x, offset_y);

				break;
			case 'p':
				if(strlen(optarg) != 2) {
					fprintf(stderr, "Invalid position, use --help for more details\n");
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
					fprintf(stderr, "Can't apply effect without loading an image first (--input)\n");
					exit(1);
				}

				t = (int)strtol(optarg, NULL, 10);
				mj_effect_luminance(m, t);
				break;
			case 'b':
				if(m == NULL) {
					fprintf(stderr, "Can't apply effect without loading an image first (--input)\n");
					exit(1);
				}

				t = (int)strtol(optarg, NULL, 10);
				mj_effect_tint(m, t, 0);
				break;
			case 'r':
				if(m == NULL) {
					fprintf(stderr, "Can't apply effect without loading an image first (--input)\n");
					exit(1);
				}

				t = (int)strtol(optarg, NULL, 10);
				mj_effect_tint(m, 0, t);
				break;
			case 'x':
				if(m == NULL) {
					fprintf(stderr, "Can't apply effect without loading an image first (--input)\n");
					exit(1);
				}

				mj_effect_pixelate(m);
				break;
			case 'g':
				if(m == NULL) {
					fprintf(stderr, "Can't apply effect without loading an image first (--input)\n");
					exit(1);
				}
				
				mj_effect_grayscale(m);
				break;
			case 'O':
				options |= MJ_OPTION_OPTIMIZE;
				break;
			case 'P':
				options |= MJ_OPTION_PROGRESSIVE;
				break;
			case 'h':
				help();
				exit(0);
			case ':':
				fprintf(stderr, "Argument missing, use --help for more details\n");
				break;
			case '?':
			default:
				fprintf(stderr, "Unknown option, use --help for more details\n");
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

	fprintf(stderr, "The order for the options is important, i.e. a dropon can't be applied without\n");
	fprintf(stderr, "loading an image first.\n\n");

	fprintf(stderr, "Options:\n\n");

	fprintf(stderr, "\t--input, -i file\n");
	fprintf(stderr, "\t\tPath to the image to be modified. The image needs to be a JPEG.\n");
	fprintf(stderr, "\n");

	fprintf(stderr, "\t--ouput, -o file\n");
	fprintf(stderr, "\t\tPath to a file to store the modified image in.\n");
	fprintf(stderr, "\n");

	fprintf(stderr, "\t--dropon, -d file[,mask]\n");
	fprintf(stderr, "\t\tPath to the image that should be used as dropon. The path to the mask is optional.\n");
	fprintf(stderr, "\t\tThe dropon and the mask have to be a JPEG and of the same dimension.\n");
	fprintf(stderr, "\n");

	fprintf(stderr, "\t--position, -p [t|b][c][l|r]\n");
	fprintf(stderr, "\t\tThe position of the dropon. t = top, b = bottom, l = left, r = right, c = center. Default: center\n");
	fprintf(stderr, "\n");

	fprintf(stderr, "\t--offset, -m [horizontal],[vertical]\n");
	fprintf(stderr, "\t\tThe offset to the given position in pixels. Default: 0,0\n");
	fprintf(stderr, "\n");

	fprintf(stderr, "\t--luminance, -y value\n");
	fprintf(stderr, "\t\tChanges the brightness of the image according to the value. Use a negative value\n");
	fprintf(stderr, "\t\tto darken the image, and a positive value to brighten the image.\n");
	fprintf(stderr, "\n");

	fprintf(stderr, "\t--tintblue, -b value\n");
	fprintf(stderr, "\t\tColor the image. Use a negative value to tint the image yellow, and use a positive\n");
	fprintf(stderr, "\t\tvalue to tint the image blue.\n");
	fprintf(stderr, "\n");

	fprintf(stderr, "\t--tintred, -r value\n");
	fprintf(stderr, "\t\tColor the image. Use a negative value to tint the image green, and use a positive\n");
	fprintf(stderr, "\t\tvalue to tint the image red.\n");
	fprintf(stderr, "\n");

	fprintf(stderr, "\t--pixelate, -x\n");
	fprintf(stderr, "\t\tPixelate the image into 8x8 blocks.\n");
	fprintf(stderr, "\n");

	fprintf(stderr, "\t--grayscale, -g\n");
	fprintf(stderr, "\t\tReduce the image to grayscale.\n");
	fprintf(stderr, "\n");

	fprintf(stderr, "Examples:\n");
	fprintf(stderr, "\n");

	fprintf(stderr, "\tPlace a logo in the top right corner:\n");
	fprintf(stderr, "\t\tmodjpeg --input in.jpg --position tr --dropon logo.jpg --output out.jpg\n");
	fprintf(stderr, "\n");

	fprintf(stderr, "\tPixelate the image and then place a logo in the top right corner:\n");
	fprintf(stderr, "\t\tmodjpeg --input in.jpg --pixelate --position tr --dropon logo.jpg --output out.jpg\n");
	fprintf(stderr, "\n");

	fprintf(stderr, "\tPlace a logo in the top right corner and then pixelate the image (including the logo):\n");
	fprintf(stderr, "\t\tmodjpeg --input in.jpg --position tr --dropon logo.jpg --pixelate --output out.jpg\n");
	fprintf(stderr, "\n");

	fprintf(stderr, "\n");
	return;
}

