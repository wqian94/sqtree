export

SHELL := /bin/bash

CC := gcc
CFLAGS := -std=gnu99 -g -pthread -lpthread
RM := rm -f
CPU_NODE := 0
NOW := $(shell date -u +%s)


# to turn on compiling in parallel
ifdef PARALLEL
CCFLAGS += -DPARALLEL
endif

# to turn on mtrace
ifdef MTRACE
CCFLAGS += -DMTRACE
endif

# for NUM_THREADS
ifdef NTHREADS
CCFLAGS += -DNTHREADS=$(NTHREADS)
endif

# for INITIAL
ifdef INITIAL
CCFLAGS += -DINITIAL=$(INITIAL)
endif

# for DIMENSIONS
DIMENSIONS ?= 2
CCFLAGS += -DDIMENSIONS=$(DIMENSIONS)

TIME ?= 1# 1 second
WRATIO ?= 0.1
DRATIO ?= 0.5

# for verboseness
ifdef VERBOSE
CCFLAGS += -DVERBOSE
endif

CCFLAGS += -DTIME=$(TIME)LL -DWRATIO=$(WRATIO) -DDRATIO=$(DRATIO) \
	-DHEADER=\"Quadtree.h\" -DTYPE=Quadtree \
	-DCONSTRUCTOR=Quadtree_init -DDESTRUCTOR=Quadtree_free \
	-DINSERT=Quadtree_add -DQUERY=Quadtree_search -DDELETE=Quadtree_remove

.PHONY: all
all: benchmark.o
	@echo "Object file created: benchmark.o"

%.o: %.c
	@str="Using: `$(CC) -v 2>&1 | tail -n 1`";\
	len=$$(($${#str} + 5));\
	range=$$(eval echo {1..$$len});\
	printf "#%.s" $$range;\
	printf "\n## %s##\n" "$$str";\
	printf "#%.s" $$range;\
	printf "\n";
	$(CC) $(CFLAGS) $(CCFLAGS) -c $< -o $@

.PHONY: clean
clean:
	-$(RM) benchmark.o
