#------------------------------------------------------------------------------
# mod_jpeg Makefile
# 
# Die folgende Zeile entsprechend anpassen
#------------------------------------------------------------------------------
JPEGDIR = /usr/local

# Ab hier ist nichts mehr anzupassen

INCLUDEJPEG = -I$(JPEGDIR)/include
LIBRARYJPEG = -L$(JPEGDIR)/lib

INCLUDE = -I. $(INCLUDEJPEG)
LIBDIR = $(LIBRARYJPEG)

LIBS = -lm -ljpeg

#COMPILERFLAGS_SHARED = -Wall -O2 -fPIC -c
#COMPILERFLAGS_STATIC = -Wall -O2 -c
COMPILERFLAGS = -Wall -O2 -c -fPIC

#LINKERFLAGS_SHARED = -shared
#LINKERFLAGS_STATIC =
LINKERFLAGS = -shared

CC = gcc
CFLAGS = $(COMPILERFLAGS) $(INCLUDE)
LFLAGS = $(LINKERFLAGS) $(LIBDIR) $(LIBS)

OBJECTS = convolve.o \
          dct.o \
          effect.o \
          image.o \
          init.o \
          jpeg.o \
          logo.o \
          matrix.o \
          memory.o \
          read.o \
          resample.o \
          watermark.o

APPS = test \
       modjpeg

libmodjpeg: $(OBJECTS)
	$(CC) $(LFLAGS) $(OBJECTS) -Wl,-soname -Wl,libmodjpeg.so -o libmodjpeg.so
#	ar rv libmodjpeg.a $(OBJECTS)

apps: $(APPS)

#install: libmodjpeg.so
#	cp libmodjpeg.so $(INSTALLPATH)

convolve.o: convolve.c libmodjpeg.h convolve.h
	$(CC) $(CFLAGS) convolve.c

dct.o: dct.c libmodjpeg.h dct.h
	$(CC) $(CFLAGS) dct.c

effect.o: effect.c libmodjpeg.h effect.h
	$(CC) $(CFLAGS) effect.c

image.o: image.c libmodjpeg.h image.h jpeg.h
	$(CC) $(CFLAGS) image.c

init.o: init.c libmodjpeg.h init.h resample.h image.h logo.h watermark.h
	$(CC) $(CFLAGS) init.c

jpeg.o: jpeg.c libmodjpeg.h jpeg.h
	$(CC) $(CFLAGS) jpeg.c

logo.o: logo.c libmodjpeg.h logo.h convolve.h resample.h read.h memory.h
	$(CC) $(CFLAGS) logo.c

matrix.o: matrix.c libmodjpeg.h matrix.h
	$(CC) $(CFLAGS) matrix.c

memory.o: memory.c libmodjpeg.h memory.h
	$(CC) $(CFLAGS) memory.c

read.o: read.c libmodjpeg.h read.h memory.h dct.h resample.h jpeg.h
	$(CC) $(CFLAGS) read.c

resample.o: resample.c libmodjpeg.h resample.h memory.h matrix.h
	$(CC) $(CFLAGS) resample.c

watermark.o: watermark.c libmodjpeg.h watermark.h convolve.h resample.h read.h memory.h
	$(CC) $(CFLAGS) watermark.c

test: test.c libmodjpeg.a
	$(CC) -Wall -O2 -o test test.c -L. $(LFLAGS) -lmodjpeg $(INCLUDE)

modjpeg: modjpeg.c libmodjpeg.a
	$(CC) -Wall -O2 -o modjpeg modjpeg.c -L. $(LFLAGS) -lmodjpeg $(INCLUDE)

clean:
	rm -f *.o *~
	rm -f libmodjpeg.a

