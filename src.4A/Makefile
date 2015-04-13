RM        := /bin/rm -rf
SIM       := ./sim
CFLAGS    := -O2 -lm -std=gnu99 -W -Wall -Wno-unused-parameter
DFLAGS    := -pg -g -lm
PFLAGS    := -pg
FLAGS     := -lz
OS 	  := $(shell uname)
ifeq ($(OS),Darwin)
	CC = clang
else
	CC = gcc
endif

all: ; ${CC} ${CFLAGS} ${DFLAGS} core.c cache.c  sim.c memsys.c   -o ${SIM} ${FLAGS} -lm


clean: ; $(RM) ${SIM} *.o 

