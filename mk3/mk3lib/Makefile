SHELL = /bin/sh

LIB   = libmk3lib.a
ALLINC = 

OBJS = mk3lib.o mk3error.o mk3msg.o mk3array.o mk3_misc.o databuffers.o mk3cursor.o
SRCS = mk3lib.c mk3error.c mk3msg.c mk3array.c mk3_misc.c databuffers.c mk3cursor.c
HDRS = mk3lib.h mk3lib_int.h mk3error.h mk3msg.h mk3msg_type.h mk3array.h mk3_common.h mk3_misc.h mk3cursor.h mk3cursor_type.h


CC=gcc
CXX=g++
CFLAGS=-D_THREAD_SAFE
CXXFLAGS=

all: ${LIB}

.SUFFIXES: .cc .hh .o .a

${LIB}: ${OBJS}
	rm -f $(LIB)
	ar crs $(LIB) $(OBJS)

clean:
	rm -f ${LIB} ${OBJS}

distclean:
	rm -f ${LIB} ${OBJS} Makefile

.c.o:
	${CC} -c ${CFLAGS} ${DEFS} ${ALLINC} $<

.cc.o:
	${CXX} -c ${CXXFLAGS} ${DEFS} ${ALLINC} $<

# Quick and dirty Dependences
mk3lib.o:      ${HDRS} mk3lib.c
mk3error.o:    ${HDRS} mk3error.c
mk3msg.o:      ${HDRS} mk3msg.c
mk3array.o:    ${HDRS} mk3array.c
mk3cursor.o:   ${HDRS} mk3cursor.c
mk3_misc.o:    ${HDRS} mk3_misc.c
databuffers.o: ${HDRS} databuffers.c
