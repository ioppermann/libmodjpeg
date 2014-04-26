#include <stdio.h>
#include <string.h>

#include "libmodjpeg.h"

#define WMSIZE	64

int main(int argc, char *argv[])
{
	FILE *fp;
	int wid, lid, i;
	char wm[WMSIZE * WMSIZE];
	char logo[3 * WMSIZE * WMSIZE];
	char logoa[4 * WMSIZE * WMSIZE];
	modjpeg mj;

	if(argc < 3) {
		printf("%s dst src\n", argv[0]);
		return -1;
	}

	for(i = 0; i < WMSIZE; i++)
		memset(&wm[i * WMSIZE], (i % 2) * 0xff, WMSIZE);

	fp = fopen("face.ppm", "rb");
	fread(logo, 3 * WMSIZE * WMSIZE, 1, fp);
	rewind(fp);
	for(i = 0; i < (WMSIZE * WMSIZE); i++) {
		fread(&logoa[i * 4], 3, 1, fp);
		logoa[(i * 4) + 3] = 0x80;
	}
	fclose(fp);

	modjpeg_init(&mj);

	//lid = modjpeg_set_logo_file(&mj, argv[4], argv[5]);
	lid = modjpeg_set_logo_raw(&mj, logoa, WMSIZE, WMSIZE, MODJPEG_RGBA);

	//wid = modjpeg_set_watermark_file(&mj, argv[3]);
	wid = modjpeg_set_watermark_raw(&mj, wm, WMSIZE, WMSIZE, MODJPEG_Y);

	modjpeg_set_image_file(&mj, argv[2]);

	modjpeg_effect_pixelate(&mj);
	modjpeg_effect_grayscale(&mj);
	modjpeg_add_logo(&mj, lid, MODJPEG_BOTTOM | MODJPEG_RIGHT);
	modjpeg_add_watermark(&mj, wid);

	modjpeg_get_image_file(&mj, argv[1], MODJPEG_OPTIMIZE);

	modjpeg_destroy(&mj);

	return 0;
}
