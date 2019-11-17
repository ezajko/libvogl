# Makefile for vogl 
# Usage: make
#
# As well as selecting the devices, you may have to change LLIBS and MLIBS as well (see below).
#
SHELL = /bin/sh
#
CC = gcc

#  The devices you wish to have compiled into the library and the location of the object files 
#  for each device relative to the src directory. For each device defined there should be a 
#  corresponding ../drivers/file.o in the DOBJS line.
#
#  Possible DEVICES and their object files are:
#		-DPOSTSCRIPT	../drivers/ps.o
#		-DHPGL		../drivers/hpdxy.o
#		-DDXY		../drivers/hpdxy.o
#		-DTEK		../drivers/tek.o
#		-DSUN		../drivers/sun.o	(Sunview that is)
#		-DX11		../drivers/X11.o
#		-DNeXT		../drivers/NeXT.o	(NeXTStep)
#
DEVICES = -DPOSTSCRIPT -DX11
DOBJS = ../drivers/ps.o ../drivers/X11.o
#
# Where the fonts a going to live (For making the Hershey library)
# (Remember /tmp usually gets cleared from time to time)
#
LIB = libvogl.a
HLIB = libhershey.a 
FONTLIB = /tmp/vogl
DEST = /tmp/vogl
INCDEST = /tmp/vogl
DIST = Dist


# RanLib
RANLIB = ranlib

# Set any Special floating point options here...
FLOATING_POINT = -ffast-math

#
# Global CFLAGS can be set here.
#
# The default
CFLAGS = -O -I/usr/local/R5/include -I/usr/include -g -O3 -fomit-frame-pointer -finline-functions -fexpensive-optimizations -DNO_MULTIBUF

#
# Define F77 if you want the f77 examples.
F77 = f77
# You also define your f77 flags here too. These are the ones we used on sun
FFLAGS = -O -w /usr/lib64/libm.a

#
# The name of the library to install and where to put it.
#

# X11
LLIBS = -lX11

LIBM = -lm

LIBS = $(LLIBS) $(LIBM)

MCFLAGS = $(CFLAGS) $(FLOATING_POINT)
MFFLAGS = $(FFLAGS)
#
all:
	cd src; make -f Makefile \
			CC="$(CC)" \
			DEVICES="$(DEVICES)" \
			MCFLAGS="$(MCFLAGS)" \
			DOBJS="$(DOBJS)"\
			RANLIB="$(RANLIB)"

	cd hershey/src; make -f Makefile \
			CC="$(CC)" \
			FONTLIB="$(FONTLIB)" \
			MCFLAGS="$(MCFLAGS)" \
			LIBS="$(LIBS)" \
			RANLIB="$(RANLIB)"

	cd examples; make -f Makefile \
			CC="$(CC)" \
			MCFLAGS="$(MCFLAGS)" \
			LIBS="$(LIBS)"

	if test -n "$(F77)" ; \
	then cd examples; make -f Makefile.f77 \
			LIBS="$(LIBS)" \
			MFFLAGS="$(MFFLAGS)" \
			F77="$(F77)" ; \
	fi ; exit 0
fonts: 
	cd hershey/src; make -f Makefile \
                        CC="$(CC)" \
                        FONTLIB="$(FONTLIB)" \
                        MCFLAGS="$(MCFLAGS)" \
                        LIBS="$(LIBS)" \
                        RANLIB="$(RANLIB)"
#
install:
	cp src/$(LIB) $(DEST)
	cp hershey/src/$(HLIB) $(DEST)
	cp src/*.h $(INCDEST)
	chmod 644 $(DEST)/$(LIB)
	chmod 644 $(DEST)/$(HLIB)
	$(RANLIB) $(DEST)/$(LIB)
	$(RANLIB) $(DEST)/$(HLIB)
	cp -r $(DEST) $(DIST) 
#
clean:
	cd src; make DOBJS="$(DOBJS)" clean
	cd hershey/src; make FONTLIB="$(FONTLIB)" clean
	cd drivers; make clean
	cd examples; make clean
	cd examples/xt; make clean
	cd examples/xview; make clean
	cd examples/sunview; make clean
	cd examples; make -f Makefile.f77 clean
#
