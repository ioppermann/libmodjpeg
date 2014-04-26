/*
 * modjpeg.c
 *
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
	{ "watermark",	required_argument,	NULL,		'w' },
	{ "logo",	required_argument,	NULL,		'l' },
	{ "position",	required_argument,	NULL,		'p' },
	{ "luminance",	required_argument,	NULL,		'y' },
	{ "tintblue",	required_argument,	NULL,		'b' },
	{ "tintred",	required_argument,	NULL,		'r' },
	{ "pixelate",	no_argument,		NULL,		'x' },
	{ "grayscale",	no_argument,		NULL,		'g' },
	{ "help",	no_argument,		NULL,		'h' },
	{ NULL,		0,			NULL,		0 }
};


#define CMD_WATERMARK		1
#define CMD_LOGO		2
#define CMD_LOGOPOSITION	3
#define CMD_LUMINANCE		4
#define CMD_TINTBLUE		5
#define CMD_TINTRED		6
#define CMD_PIXELATE		7
#define CMD_GRAYSCALE		8

typedef struct commands {
	int type;
	int value;
} commands;

typedef commands *commands_ptr;

int add_command(commands *cmds, int type, int value);
int exec_commands(commands *cmds, modjpeg_handle *m);
void help(void);

int main(int argc, char *argv[]) {
	int c, opterr, mustargs, t;
	char *finput = NULL, *foutput = NULL, *str;
	modjpeg mj;
	modjpeg_handle *m;
	commands cmds[32];

	memset(cmds, 0, 32 * sizeof(commands));

	opterr = 1;
	mustargs = 0;

	modjpeg_init(&mj);

	while((c = getopt_long(argc, argv, "i:o:w:l:p:y:b:r:xgh", longopts, NULL)) != -1) {
		switch(c) {
			case 'i':
				finput = optarg;

				if(strlen(finput) == 1 && finput[0] == '-')
					finput = NULL;

				break;
			case 'o':
				foutput = optarg;

				if(strlen(foutput) == 1 && foutput[0] == '-')
					foutput = NULL;

				break;
			case 'w':
				t = modjpeg_set_watermark_file(&mj, optarg);
				add_command(cmds, CMD_WATERMARK, t);
				break;
			case 'l':
				str = strchr(optarg, ',');
				if(str != NULL) {
					*str = '\0';
					t = modjpeg_set_logo_file(&mj, optarg, str + 1);
				}
				else
					t = modjpeg_set_logo_file(&mj, optarg, NULL);
				add_command(cmds, CMD_LOGO, t);
				break;
			case 'p':
				if(strlen(optarg) != 2) {
					fprintf(stderr, "Falsche Position, --help für mehr Informationen\n");
					break;
				}

				t = 0;
				if(optarg[0] == 't')
					t |= MODJPEG_TOP;
				else if(optarg[0] == 'b')
					t |= MODJPEG_BOTTOM;

				if(optarg[1] == 'l')
					t |= MODJPEG_LEFT;
				else if(optarg[1] == 'r')
					t |= MODJPEG_RIGHT;

				add_command(cmds, CMD_LOGOPOSITION, t);
				break;
			case 'y':
				t = (int)strtol(optarg, NULL, 10);
				add_command(cmds, CMD_LUMINANCE, t);
				break;
			case 'b':
				t = (int)strtol(optarg, NULL, 10);
				add_command(cmds, CMD_TINTBLUE, t);
				break;
			case 'r':
				t = (int)strtol(optarg, NULL, 10);
				add_command(cmds, CMD_TINTRED, t);
				break;
			case 'x':
				add_command(cmds, CMD_PIXELATE, 0);
				break;
			case 'g':
				add_command(cmds, CMD_GRAYSCALE, 0);
				break;
			case 'h':
				help();
				exit(0);
			case ':':
				fprintf(stderr, "Argument fehlt, --help für mehr Informationen\n");
				break;
			case '?':
			default:
				fprintf(stderr, "Unbekannte Option, --help für mehr Informationen\n");
				break;
		}
	}

	if(finput != NULL)
		m = modjpeg_set_image_file(&mj, finput);
	else
		m = modjpeg_set_image_fp(&mj, stdin);

	exec_commands(cmds, m);

	if(foutput != NULL)
		modjpeg_get_image_file(m, foutput, MODJPEG_OPTIMIZE);
	else
		modjpeg_get_image_fp(m, stdout, MODJPEG_OPTIMIZE);

	modjpeg_destroy(&mj);

	return 0;
}

int add_command(commands *cmds, int type, int value) {
	int i;
	commands *cmd = NULL;

	for(i = 0; i < 32; i++) {
		if(cmds[i].type == 0) {
			cmd = &cmds[i];
			break;
		}
	}

	if(cmd == NULL)
		return -1;

	cmd->type = type;
	cmd->value = value;

	return 0;
}

int exec_commands(commands *cmds, modjpeg_handle *m) {
	int i, logoposition = 0;
	commands *cmd;

	for(i = 0; i < 32; i++) {
		cmd = &cmds[i];

		if(cmd->type == 0)
			break;

		switch(cmd->type) {
			case CMD_WATERMARK:
				modjpeg_add_watermark(m, cmd->value);
				break;
			case CMD_LOGO:
				modjpeg_add_logo(m, cmd->value, logoposition);
				break;
			case CMD_LOGOPOSITION:
				logoposition = cmd->value;
				break;
			case CMD_LUMINANCE:
				modjpeg_effect_luminance(m, cmd->value);
				break;
			case CMD_TINTBLUE:
				modjpeg_effect_tint(m, cmd->value, 0);
				break;
			case CMD_TINTRED:
				modjpeg_effect_tint(m, 0, cmd->value);
				break;
			case CMD_PIXELATE:
				modjpeg_effect_pixelate(m);
				break;
			case CMD_GRAYSCALE:
				modjpeg_effect_grayscale(m);
				break;
			default:
				break;
		}
	}

	return 0;
}

void help(void) {
	fprintf(stderr, "modjpeg (c) 2006 Ingo Oppermann\n\n");
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

	fprintf(stderr, "\t--watermark, -w file\n");
	fprintf(stderr, "\t\tName des Bildes, das als Wasserzeichen verwendet werden soll. Das Bild\n");
	fprintf(stderr, "\t\tmuss im JPEG-Format sein.\n");
	fprintf(stderr, "\n");

	fprintf(stderr, "\t--logo, -l file[,mask]\n");
	fprintf(stderr, "\t\tName des Bildes, das als Logo verwendet werden soll. Die Maske ist optional.\n");
	fprintf(stderr, "\t\tLogo und Maske müssen im JPEG-Format sein.\n");
	fprintf(stderr, "\n");

	fprintf(stderr, "\t--position, -p [t|b][l|r]\n");
	fprintf(stderr, "\t\tDie Position des Logos. t = top, b = bottom, l = left, r = right.\n");
	fprintf(stderr, "\n");

	fprintf(stderr, "\t--luminance, -y value\n");
	fprintf(stderr, "\t\tVerändert die Helligkeit des Bildes entsprechend des Wertes in value.\n");
	fprintf(stderr, "\n");

	fprintf(stderr, "\t--tintblue, -b value\n");
	fprintf(stderr, "\t\tEin positiver Wert färbt das Bild blau, ein negativer Wert färbt\n");
	fprintf(stderr, "\t\tdas Bild gelb.\n");
	fprintf(stderr, "\n");

	fprintf(stderr, "\t--tintred, -r value\n");
	fprintf(stderr, "\t\tEin positiver Wert färbt das Bild rot, ein negativer Wert färbt\n");
	fprintf(stderr, "\t\tdas Bild grün.\n");
	fprintf(stderr, "\n");

	fprintf(stderr, "\t--pixelate, -x\n");
	fprintf(stderr, "\t\tVerpixelt das Bild in 8x8-Blöcken.\n");
	fprintf(stderr, "\n");

	fprintf(stderr, "\t--grayscale, -g\n");
	fprintf(stderr, "\t\tReduziert das Bild zu Graustufen.\n");
	fprintf(stderr, "\n");

	fprintf(stderr, "Beispiele:\n");
	fprintf(stderr, "\n");

	fprintf(stderr, "\tEin Logo in der rechten oberen Ecke platzieren:\n");
	fprintf(stderr, "\t\tmodjpeg --position tr --logo logo.jpg < in.jpg > out.jpg\n");
	fprintf(stderr, "\n");

	fprintf(stderr, "\tEin Logo in der rechten oberen Ecke platzieren und nachher das Bild\n");
	fprintf(stderr, "\tverpixeln:\n");
	fprintf(stderr, "\t\tmodjpeg --position tr --logo logo.jpg --pixelate < in.jpg > out.jpg\n");
	fprintf(stderr, "\n");

	fprintf(stderr, "\tErst das Bild verpixeln, dann ein Logo in der rechten oberen Ecke\n");
	fprintf(stderr, "\tplatzieren:\n");
	fprintf(stderr, "\t\tmodjpeg --pixelate --position tr --logo logo.jpg < in.jpg > out.jpg\n");
	fprintf(stderr, "\n");

	fprintf(stderr, "\n");
	return;
}

