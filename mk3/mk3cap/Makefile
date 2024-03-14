MK3LIB_PATH=../mk3lib

ALLINC = -I${MK3LIB_PATH}

ALLLIB = -L${MK3LIB_PATH} -lmk3lib -lpthread

DEPLIB = ${DEPLIB1} ${MK3LIB_PATH}/libmk3lib.a

PROG  = mk3cap

PROGS = ${PROG}

OBJS = mk3cap_lib_raw_nosf.o
SRCS = mk3cap_lib_raw_nosf.c

HDRS =

CC=gcc
CXX=g++
CFLAGS=-D_THREAD_SAFE -D_FILE_OFFSET_BITS=64
CXXFLAGS=-D_THREAD_SAFE -D_FILE_OFFSET_BITS=64

all: ${PROGS}

.SUFFIXES: .cc .hh .o .a

${PROG}: ${OBJS} ${DEPLIB}
	${CC} ${CFLAGS} ${OBJS} ${ALLLIB} ${DEPLIB} -o ${PROG}

clean:
	-rm -f $(PROG) ${PROGS} ${OBJS}

distclean:
	-rm -f $(PROG) ${PROGS} ${OBJS} Makefile

.c.o:
	${CC} -c ${CFLAGS} ${DEFS} ${ALLINC} $<

.cc.o:
	${CXX} -c ${CXXFLAGS} ${DEFS}  $<
