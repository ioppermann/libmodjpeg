# libmodjpeg

A library for JPEG masking and composition in the DCT domain.

- [Background](#background)
- [Compiling and installing](#compiling-and-installing)
- [Synopsis](#synopsis)
  - [Header](#header)
  - [Dropon](#dropon)
  - [Image](#image)
  - [Composition](#composition)
  - [Effect](#effect)
  - [Return values](#return-values)
  - [Supported color spaces](#supported-color-spaces)
- [Example](#example)
- [License](#license)
- [Acknowledgement](#acknowledgement)
- [References](#references)


## Background

With libmodjpeg you can overlay a (masked) image onto an existing JPEG as lossless as possible. Changes in the JPEG only
take place where the overlayed image is applied. All modifications happen in the DCT domain, thus the JPEG is decoded and
encoded losslessly.

Adding an overlay (e.g. logo, watermark, ...) to an existing JPEG image usually will result in loss of quality because the JPEG
needs to get decoded and then re-encoded after the overlay has been applied. [Read more about JPEG on Wikipedia](https://en.wikipedia.org/wiki/JPEG)

libmodjpeg avoids the decoding and re-encoding of the JPEG image by applying the overlay directly on the un-transformed DCT
coefficients. Only the area where the overlay is applied to is affected by changes and the rest of the image will remain untouched.

The usual process of applying a (masked) overlay involved these steps:

1. Huffman decode
2. de-quantize
3. inverse DCT *
3. colorspace transformation from YCbCr to RGB *
4. applying the (masked) overlay
5. colorspace transformation from RGB to YCbCr *
6. DCT *
7. quantize *
8. Huffman encode

The steps marked with a * will lead to loss of quality.

libmodjpeg avoids all lossy steps by applying the (masked) overlay directly in the DCT domain:

1. Huffman decode
2. de-quantize
3. applying the (masked) overlay
4. quantize
5. Huffman encode

In step 4, the quantization is lossless compared to the usual process because the same DCT and quantization
values are used as in step 2.

Only the overlay itself will experience a loss of quality because it needs to be transformed into the DCT domain
with the same colorspace and sampling as the image it will be applied to.

## Compiling and installing

libmodjpeg requires the [libjpeg](http://www.ijg.org/) v9 6? 7? 8? or compatible ([libjpeg-turbo](https://github.com/libjpeg-turbo/libjpeg-turbo)
or [mozjpeg](https://github.com/mozilla/mozjpeg)), however the IJG libjpeg or
libjpeg-turbo are recommended because mozjpeg will always produce progressive JPEGs which is slower and may not be desired.

```
# git clone https://github.com/ioppermann/libmodjpeg.git
# cd libmodjpeg
# cmake .
# make
# make install
```

In case libjpeg (or compatible) are installed in a non-standard location you can set the environment variable `CMAKE_PREFIX_PATH`
to the location where the libjpeg is installed, e.g.:

```
# env CMAKE_PREFIX_PATH=/usr/local/opt/jpeg-turbo/ cmake .
```

## Synopsis

### Header

```C
#include <libmodjpeg.h>
```

Include the header file in order to have access to the library.

### Dropon

```C
struct mj_dropon_t;
```

A "dropon" denotes the overlay that will be applied to an image and is defined by the struct `mj_dropon_t`.

```C
void mj_init_dropon(mj_dropon_t *d);
```

Initialize the dropon in order to make it ready for use.

```C
int mj_read_dropon_from_raw(
	mj_dropon_t *d,
	const char *rawdata,
	unsigned int colorspace,
	size_t width,
	size_t height,
	short blend);
```

Read a dropon from raw data. The raw data is a pointer to an array of chars holding the raw image data in the given color space

```C
#define MJ_COLORSPACE_RGB            1  // [0] = R0, [1] = G0, [2] = B0, [3] = R1, ...
#define MJ_COLORSPACE_RGBA           2  // [0] = R0, [1] = G0, [2] = B0, [3] = A0, [4] = R1, ...
#define MJ_COLORSPACE_GRAYSCALE      3  // [0] = Y0, [1] = Y1, ...
#define MJ_COLORSPACE_GRAYSCALEA     4  // [0] = Y0, [1] = A0, [2] = Y1, ...
#define MJ_COLORSPACE_YCC            5  // [0] = Y0, [1] = Cb0, [2] = Cr0, [3] = Y1, ...
#define MJ_COLORSPACE_YCCA           6  // [0] = Y0, [1] = Cb0, [2] = Cr0, [3] = A0, [4] = Y1, ...
```

`width` and `height` are the dimensions of the raw image. `blend` is a value in [0, 255] for the translucency for the dropon if no alpha
channel is given, where 0 is fully transparent (the dropon will not be applied) and 255 is fully opaque.

```C
int mj_read_dropon_from_jpeg_file(
	mj_dropon_t *d,
	const char *filename,
	const char *maskfilename,
	short blend);
```

Read a dropon from a JPEG file (`filename`). The alpha channel is given by the second JPEG file (`maskfilename`). Use `NULL` if no alpha
channel is available or wanted. `blend` is a value for the translucency for the dropon if no alpha channel is given.

```C
int mj_read_dropon_from_jpeg_bistream(
	mj_dropon_t *d,
	const char *bitstream,
	size_t len,
	const char *maskbitstream,
	size_t masklen,
	short blend);
```

Read a dropon from a JPEG bistream (`bitstream` of `len` bytes length). The alpha channel is given by the second JPEG bitstream (`maskbitstream` of `masklen` bytes length).
Use `NULL` for `maskbitstream` or `0` for `masklen` if no alpha channel is available or wanted. `blend` is a value for the translucency for the dropon if no alpha channel is given.

```C
void mj_free_dropon(mj_dropon_t *d);
```

Free the memory consumed by the dropon. The dropon struct can be reused for another dropon.

### Image

```C
struct mj_jpeg_t;
```
The `mj_jpeg_t` holds the JPEG a dropon can be applied to.

```C
void mj_init_jpeg(mj_jpeg_t *m);
```
Initialize the image in order to make it ready for use.

```C
int mj_read_jpeg_from_bitstream(
	mj_jpeg_t *m,
	const char *bitstream,
	size_t len,
	size_t max_pixel);
```
Read a JPEG from a buffer. The buffer holds the JPEG bitstream of length `len` bytes. `max_pixel` is the maximum number of pixel allowed in the image
to prevent processing too big images. Set it to `0` to allow any sized images.

```C
int mj_read_jpeg_from_file(
	mj_jpeg_t *m,
	const char *filename,
	size_t max_pixel);
```
Read a JPEG from a file denoted by `filename`. `max_pixel` is the maximum number of pixel allowed in the image
to prevent processing too big images. Set it to `0` to allow any sized images.

```C
int mj_write_jpeg_to_bitstream(
	mj_jpeg_t *m,
	char **bitstream,
	size_t *len,
	int options);
```
Write an image to a buffer as a JPEG bitstream. The required memory for the buffer will be allocated and must be free'd after use. `len` holds
the length of the buffer in bytes. `options` are encoding features that can be OR'ed:

* `MJ_OPTION_NONE` - baseline encoding
* `MJ_OPTION_OPTIMIZE` - optimize Huffman tables
* `MJ_OPTION_PROGRESSIVE` - progressive encoding
* `MJ_OPTION_ARITHMETRIC` - arithmetric encoding (overrules Huffman optimizations)

```C
int mj_write_jpeg_to_file(
	mj_jpeg_t *m,
	char *filename,
	int options);
```
Write an image to a file as a JPEG bitstream. The options are the same as for `mj_write_jpeg_to_buffer()`.

```C
void mj_free_jpeg(mj_jpeg_t *m);
```
Free the memory consumed by the JPEG. The jpeg struct can be reused for another image.

### Composition

```C
int  mj_compose(
	mj_jpeg_t *m,
	mj_dropon_t *d,
	unsigned int align,
	int offset_x,
	int offset_y);
```
Compose an image with a dropon. Use these OR'ed values for `align`:

* `MJ_ALIGN_LEFT` - align the dropon to the left border of the image
* `MJ_ALIGN_RIGHT` - align the dropon to the right border of the image
* `MJ_ALIGN_TOP` - align the dropon to the top border of the image
* `MJ_ALIGN_BOTTOM` - align the dropon to the bottom border of the image
* `MJ_ALIGN_CENTER` - align the dropon to the center of the image

Use `offset_x` and `offset_y` to move the dropon relative to the alignment. If parts of the dropon will be outside of the area
of the image, it will be cropped accordingly, e.g. you can apply a dropon that is bigger than the image.

### Effects

```C
int mj_effect_grayscale(mj_jpeg_t *m);
```
Convert the image to grayscale. This only works if the image was stored in YCbCr color space. It will keep all three components.

```C
int mj_effect_pixelate(mj_jpeg_t *m);
```
Keep only the DC coefficients from the components. This will remove the details from the image and keep only the "base" color of each block.

```C
int mj_effect_tint(
	mj_jpeg_t *m,
	int cb_value,
	int cr_value);
```
Colorize the image. Use `cb_value` to colorize in blue (positive value) or yellow (negative value). Use `cr_value` to colorize in
red (positive value) or green (negative value). This only works if the image was stored in YCbCr color space.

```C
int mj_effect_luminance(
	mj_jpeg_t *m,
	int value);
```
Change the brightness of the image. Use a positive value to brighten or a negative value to darken then image.
This only works if the image was stored in YCbCr color space.

### Return values

All non-void functions return `MJ_OK` if everything went fine. If something went wrong the return value indicates the source
of error:

* `MJ_ERR_MEMORY` - failed to allocate enough memory
* `MJ_ERR_NULL_DATA` - some provided pointers are NULL
* `MJ_ERR_DROPON_DIMENSIONS` - the dimensions of the dropon image and alpha do not correspond
* `MJ_ERR_UNSUPPORTED_COLORSPACE` - the JPEG is in an unsupported color space
* `MJ_ERR_DECODE_JPEG` - error during decoding the JPEG
* `MJ_ERR_ENCODE_JPEG` - error during encoding the JPEG
* `MJ_ERR_FILEIO` - error while reading/writing from/to a file
* `MJ_ERR_IMAGE_SIZE` - the dimensions of the provided image are too large 

### Supported color spaces

libmodjpeg only supports the "basic" and most common color spaces in JPEG files: `JCS_RGB`, `JCS_GRAYSCALE`, and `JCS_YCbCr`


## Example

```C
#include <libmodjpeg.h>

int main(int argc, char **argv) {
	// Initialize dropon struct
	struct mj_dropon_t d;
	mj_init_dropon(&d);

	// Read a dropon from a JPEG, without mask and with 50% translucency
	mj_read_dropon_from_jpeg_file(&d, "logo.jpg", NULL, 50);

	// Initialize JPEG image struct
	struct mj_jpeg_t m;
	mj_init_jpeg(&m);

	// Read a JPEG image from a file
	mj_read_jpeg_from_file(&m, "in.jpg", 0);

	// Place the dropon in the bottom right corner of the JPEG image
	// with 10px distance to the bottom and right border
	mj_compose(&m, &d, MJ_ALIGN_BOTTOM | MJ_ALIGN_RIGHT, -10, -10);

	// Write the JPEG image to a file with optimzed Hufman tables and progressive mode
	mj_write_jpeg_to_file(&m, "out.jpg", MJ_OPTION_OPTIMIZE | MJ_OPTION_PROGRESSIVE);

	// Free the dropon and JPEG image structs
	mj_free_jpeg(&m);
	mj_free_dropon(&d);

	return 0;
}
```

In the [contrib](../../tree/master/contrib) directory you find an example program that implements all described functionality.

```
# cd contrib
# cmake .
# make
```
In case the jpeglib (or compatible) is installed in a non-standard location, use the same environment variable for cmake as described above.


## License

libmodjpeg is covered by the MIT license. Refer to [LICENSE](/blob/master/LICENSE).


## Acknowledgement

Made with :pizza: and :beers: in Switzerland.


## References

[1] R. Jonsson, "Efficient DCT Domain Implementation of Picture Masking
    and Composition and Compositing", ICIP (2) 1997, pp. 366-369