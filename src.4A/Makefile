RM        := /bin/rm -rf
SIM       := ./sim
CC        := clang
CFLAGS    := -O2 -lm -std=gnu99 -W -Wall -Wno-unused-parameter
DFLAGS    := -pg -g
PFLAGS    := -pg
FLAGS     := -lz



all: 
	${CC} ${CFLAGS} ${DFLAGS} core.c cache.c  sim.c memsys.c   -o ${SIM} ${FLAGS}


clean: 
	$(RM) ${SIM} *.o 
