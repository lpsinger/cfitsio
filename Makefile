# Generated automatically from Makefile.in by configure.
#
# Makefile for cfitsio library:
#       libcfits.a
#
# Oct-96 : original version by 
#
#       JDD/WDP
#       NASA GSFC
#       Oct 1996
#
# 09-Mar-98 : modified to conditionally compile drvrsmem.c. Also
# changes to target all (deleted clean), added DEFS, LIBS, added
# DEFS to .c.o, added SOURCES_SHMEM and MY_SHMEM, expanded getcol*
# and putcol* in SOURCES, modified OBJECTS, mv changed to /bin/mv
# (to bypass aliasing), cp changed to /bin/cp, add smem and
# testprog targets. See also changes and comments in configure.in
# If for any reason shared memory driver cannot be compiled
# set MY_SHMEM variable to empty string and remove HAVE_SHMEM_SERVICES
# from DEFS, then recompile library.
#

SHELL =		/bin/sh
RANLIB =	ranlib
CC =		gcc
CFLAGS =	-g -O2 -Dg77Fortran
LDFLAGS =	$(CFLAGS)
DEFS =		 -DHAVE_STDLIB_H=1 -DHAVE_STRING_H=1 -DHAVE_MATH_H=1 -DHAVE_LIMITS_H=1 -DHAVE_FTRUNCATE=1 -DHAVE_SHMEM_SERVICES=1 -DHAVE_UNION_SEMUN=1 -DHAVE_NET_SERVICES=1 
LIBS =		
FLEX =		flex
BISON =		bison

.c.o:
		$(CC) -c $(CFLAGS) $(DEFS) $<


SOURCES_SHMEM =	drvrsmem.c
MY_SHMEM =	${SOURCES_SHMEM}

SOURCES = 	buffers.c cfileio.c checksum.c drvrfile.c drvrmem.c drvrnet.c \
		fitscore.c getcol.c getcolb.c getcold.c getcole.c \
		getcoli.c getcolj.c getcolk.c getcoll.c getcols.c getcoluk.c\
		getcolui.c getcoluj.c editcol.c edithdu.c getkey.c histo.c \
		modkey.c putcol.c putcolb.c putcold.c putcole.c putcoli.c \
		putcolj.c putcolk.c putcoluk.c putcoll.c putcols.c putcolu.c \
		putcolui.c putcoluj.c putkey.c scalnull.c swapproc.c \
		wcsutil.c compress.c ${FITSIO_SRC} ${MY_SHMEM} \
		eval_l.c eval_y.c eval_f.c group.c

OBJECTS = 	${SOURCES:.c=.o}

FITSIO_SRC =	f77_wrap1.c f77_wrap2.c

# ============ description of all targets =============
#       -  <<-- ignore error code

all:		stand_alone

all-nofitsio:
		${MAKE} all "FITSIO_SRC="

stand_alone:	libcfitsio.a

libcfitsio.a:	${OBJECTS}
		ar rv libcfitsio.a ${OBJECTS}; \
		${RANLIB} libcfitsio.a;

install:	libcfitsio.a
		/bin/mv libcfitsio.a ${FTOOLS_LIB}
		/bin/cp *.h ${FTOOLS_INCLUDE}/

smem:		smem.o libcfitsio.a ${OBJECTS}
		${CC} $(CFLAGS) $(DEFS) -o smem smem.o -L. -lcfitsio -lm

testprog:	testprog.o libcfitsio.a ${OBJECTS}
		${CC} $(CFLAGS) $(DEFS) -o testprog testprog.o -L. -lcfitsio -lm ${LIBS}

fitscopy:	fitscopy.o libcfitsio.a ${OBJECTS}
		${CC} $(CFLAGS) $(DEFS) -o fitscopy fitscopy.o -L. -lcfitsio -lm ${LIBS}

eval:		# Rebuild eval_* files from flex/bison source
		$(FLEX) -t eval.l > eval_l.c1
		/bin/sed -e 's/yy/ff/g' -e 's/YY/FF/g' eval_l.c1 > eval_l.c
		/bin/rm -f eval_l.c1
		$(BISON) -d -v -y eval.y
		/bin/sed -e 's/yy/ff/g' -e 's/YY/FF/g' y.tab.c > eval_y.c
		/bin/sed -e 's/yy/ff/g' -e 's/YY/FF/g' y.tab.h > eval_tab.h
		/bin/rm -f y.tab.c y.tab.h

clean:
	-	/bin/rm -f *.o libcfitsio.a smem testprog y.output

distclean:	clean
	-	/bin/rm -f Makefile config.*
