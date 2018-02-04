#------------------------------------------------------------------------------
# libmodjpeg Makefile
# 
# Adjust the following line
#------------------------------------------------------------------------------
JPEGDIR = /usr/local
INSTALLPATH = /usr/local

# Don't change anything below this line

INCLUDEJPEG = -I$(JPEGDIR)/include
LIBRARYJPEG = -L$(JPEGDIR)/lib

INCLUDE = -I. $(INCLUDEJPEG)
LIBDIR = $(LIBRARYJPEG)

LIBS = -lm -ljpeg

COMPILERFLAGS = -fPIC -O2 -c -Wall -Wextra -Wpointer-arith -Wconditional-uninitialized -Wno-unused-parameter -Wno-deprecated-declarations -Werror

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
	$(CC) $(LFLAGS) $(OBJECTS) -Wl,-dylib -Wl,-install_name,/usr/local/lib/libmodjpeg.so -Wl,-compatibility_version,1.0.0 -Wl,-current_version,1.0.0 -o libmodjpeg.so
	ar rv libmodjpeg.a $(OBJECTS)

apps: $(APPS)

install: libmodjpeg.so libmodjpeg.h
	cp libmodjpeg.so $(INSTALLPATH)/lib
	cp libmodjpeg.h $(INSTALLPATH)/include

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

modjpeg: modjpeg.c libmodjpeg.a
	$(CC) -Wall -O2 -o modjpeg modjpeg.c libmodjpeg.a -L. $(LFLAGS) $(INCLUDE)

all: libmodjpeg modjpeg

clean:
	rm -f *.o *~
	rm -f libmodjpeg.a

