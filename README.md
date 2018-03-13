libmodjpeg
==========

A library for adding overlays to JPEG images as lossless as possible.

Adding an overlay (e.g. logo, watermark, ...) to an existing JPEG image will result in loss of quality because the JPEG
needs to get decoded and then re-encoded after the overlay has been applied. This library avoids the decoding and
re-encoding of the JPEG image by applying the overlay directly on the un-decoded DCT coefficients. Only the area where
the overlay is applied is affected by changes and the rest of the image will remain untouched.

Bitstream -> iRLE -> de-quantize -> iDCT -> (YCbCr -> RGB) -> Overlay -> (RGB -> YCbCr) -> DCT -> quantize -> RLE -> Bitstream

Bitstream -> iRLE -> de-quantize -> Overlay -> quantize -> RLE -> Bitstream


`#include <libmodjpeg.h>`

mj_dropon_t *mj_read_dropon_from_buffer(const char *rawdata, unsigned int colorspace, size_t width, size_t height, short blend);
mj_dropon_t *mj_read_dropon_from_jpeg(const char *filename, const char *mask, short blend);

mj_jpeg_t *mj_read_jpeg_from_buffer(const char *buffer, size_t len);
mj_jpeg_t *mj_read_jpeg_from_file(const char *filename);

int mj_compose(mj_jpeg_t *m, mj_dropon_t *d, unsigned int align, int x_offset, int y_offset);

int mj_write_jpeg_to_buffer(mj_jpeg_t *m, char **buffer, size_t *len, int options);
int mj_write_jpeg_to_file(mj_jpeg_t *m, char *filename, int options);

void mj_destroy_jpeg(mj_jpeg_t *m);
void mj_destroy_dropon(mj_dropon_t *d);

int mj_effect_grayscale(mj_jpeg_t *m);
int mj_effect_pixelate(mj_jpeg_t *m);
int mj_effect_tint(mj_jpeg_t *m, int cb_value, int cr_value);
int mj_effect_luminance(mj_jpeg_t *m, int value);