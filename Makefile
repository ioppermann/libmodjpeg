#------------------------------------------------------------------------------
# mod_jpeg Makefile
# 
# Die folgende Zeile entsprechend anpassen
#------------------------------------------------------------------------------
JPEGDIR = /opt/local

# Ab hier ist nichts mehr anzupassen

INCLUDEJPEG = -I$(JPEGDIR)/include
LIBRARYJPEG = -L$(JPEGDIR)/lib

INCLUDE = -I. $(INCLUDEJPEG)
LIBDIR = $(LIBRARYJPEG)

LIBS = -lm -ljpeg

#COMPILERFLAGS_SHARED = -Wall -O2 -fPIC -c
#COMPILERFLAGS_STATIC = -Wall -O2 -c
COMPILERFLAGS = -Wall -O2 -c

#LINKERFLAGS_SHARED = -shared
#LINKERFLAGS_STATIC =
LINKERFLAGS = 

CC = gcc
CFLAGS = $(COMPILERFLAGS) $(INCLUDE)
LFLAGS = $(LINKERFLAGS) $(LIBDIR) $(LIBS)

OBJECTS = convolve.o \
          image.o \
          jpeg.o \
          dropon.o \
          compose.o \
          effect.o

APPS = modjpeg

libmodjpeg: $(OBJECTS)
#	$(CC) $(LFLAGS) $(OBJECTS) -Wl,-soname -Wl,libmodjpeg.so -o libmodjpeg.so
	ar rv libmodjpeg.a $(OBJECTS)

apps: $(APPS)

#install: libmodjpeg.so
#	cp libmodjpeg.so $(INSTALLPATH)

convolve.o: convolve.c libmodjpeg.h convolve.h
	$(CC) $(CFLAGS) convolve.c

image.o: image.c libmodjpeg.h image.h image.c jpeg.h jpeg.c
	$(CC) $(CFLAGS) image.c

jpeg.o: jpeg.c libmodjpeg.h jpeg.h
	$(CC) $(CFLAGS) jpeg.c

dropon.o: dropon.c libmodjpeg.h image.h image.c dropon.h
	$(CC) $(CFLAGS) dropon.c

compose.o: compose.c libmodjpeg.h dropon.h dropon.c convolve.h convolve.c compose.h
	$(CC) $(CFLAGS) compose.c

effect.o: effect.c libmodjpeg.h jpeg.h jpeg.c effect.h
	$(CC) $(CFLAGS) effect.c

modjpeg: modjpeg.c libmodjpeg
	$(CC) -Wall -O2 -o modjpeg modjpeg.c -L. $(LFLAGS) -lmodjpeg $(INCLUDE)

clean:
	rm -f *.o *~
	rm -f libmodjpeg.a

