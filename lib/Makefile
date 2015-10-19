export

SHELL := /bin/bash

CC := gcc
CFLAGS := -std=gnu99 -g -Werror -fgnu-tm
RM := rm -f
CPU_NODE := 0
NOW := $(shell date -u +%s%N)

HEADERS := \
	util.h \
	types.h \
	Point.h \
	Quadtree.h

TEST_HEADERS := \
	test.h \
	assertions.h

ALL_OBJS := Point.o util.o

.PRECIOUS: benchmark.o

# for thread counts
NTHREADS ?= 1

# for HTM
#ITM ?= rh_htm_gl_wt
#HTM_LIB ?= /home/qian/quadtree/quadtree/libitm.so
#TC_MALLOC ?= /home/qian/quadtree/quadtree/libtcmalloc.so
#TM_PRELOAD := export ITM_DEFAULT_METHOD=$(ITM);export LD_PRELOAD=$(HTM_LIB):$(TC_MALLOC):$$$$LD_PRELOAD;

# for toggling whether to run
RUN ?= 1

# to turn on compiling in parallel
ifdef PARALLEL
CCFLAGS += -DPARALLEL -pthread -lpthread
endif

# for no NUMA
ifdef NONUMA
NUMACTL := numactl --cpunodebind=$(CPU_NODE)
endif

# for sanitize
ifdef SANITIZE_ADDRESS
CC := clang
CFLAGS += -fsanitize=address
PRERUN += export ASAN_SYMBOLIZER_PATH=/usr/bin/llvm-symbolizer;
endif

# for mtrace
ifdef MTRACE
PRERUN += export MALLOC_TRACE=mtrace/mtrace.$(NOW);
POSTRUN += ;mtrace mtrace/mtrace.$(NOW);
endif

# for gprof
ifdef GPROF
CCFLAGS += -pg
endif

# for debug
ifdef DEBUG
CCFLAGS += -DDEBUG
endif

# for dimensions
DIMENSIONS ?= 3

# for timeout in benchmarking
TIMEOUT ?= 15s

# for OFLAG in benchmarking
OFLAG ?= O3

# for trial counts in benchmarking
TRIALS ?= 1

.PHONY: all
all:
	@echo -e "\
\`make all' is not valid. Here are a list of targets to use:\n\
\n\
Make targets:\n\
=============\n\
test-%: run correctness and performance tests on variant %\n\
test-%-correctness: run correctness tests on variant %\n\
test-%-performance: run performance tests on variant %\n\
benchmark-%: run benchmarks on variant %\n\
benchmark-%-O0: run benchmarks on variant % with -O0\n\
benchmark-%-O1: run benchmarks on variant % with -O1\n\
benchmark-%-O2: run benchmarks on variant % with -O2\n\
benchmark-%-O3: run benchmarks on variant % with -O3\n\
main-%: compile main program on variant %\n\
\n\
Variants:\n\
=========\n\
naive: the naive, simple implementation\n\
"

.PHONY: main-%
main-%:
	$(MAKE) -e run-main

.PHONY: test-%-correctness
test-%-correctness: CFLAGS += -O0 -DDEBUG
test-%-correctness: TESTFLAG += -DQUADTREE_TEST
test-%-correctness:
	$(MAKE) -e run-test OBJS="$(ALL_OBJS) $*/Quadtree.o" MTRACE=1 DEBUG=1

.PHONY: test-%-performance
test-%-performance: CFLAGS += -O0 -DDEBUG
test-%-performance: TESTFLAG += -DVERBOSE
test-%-performance:
	$(MAKE) -e run-test_perf OBJS="$(ALL_OBJTS) $*/Quadtree.o" GPROF=1

.PHONY: test-%
test-%:
	$(MAKE) -e test-$*-correctness
	$(MAKE) -e test-$*-performance

.PHONY: benchmark-%-O0
benchmark-%-O0:
	$(MAKE) -e benchmark-$* OFLAG="O0"

.PHONY: benchmark-%-O1
benchmark-%-O1:
	$(MAKE) -e benchmark-$* OFLAG="O1"

.PHONY: benchmark-%-O2
benchmark-%-O2:
	$(MAKE) -e benchmark-$* OFLAG="O2"

.PHONY: benchmark-%-O3
benchmark-%-O3:
	$(MAKE) -e benchmark-$* OFLAG="O3"

.PHONY: benchmark-%
benchmark-%:
	mkdir -p benchmarks/bin benchmarks/results
	$(MAKE) -e run-benchmark-benchmark OBJS="$(ALL_OBJS) $*/Quadtree.o" CFLAGS="$(CFLAGS) -$(OFLAG)"

.PHONY: run-benchmark-%
run-benchmark-%: PRERUN += export NANOSECONDS=`date +%N`;
run-benchmark-%: POSTRUN := | tee benchmarks/results/test-$(OFLAG)-$(NOW)-$$NANOSECONDS.txt;cat benchmarks/results/test-$(OFLAG)-$(NOW)-$$NANOSECONDS.txt | head -n 1 >> benchmarks/results/test-$(OFLAG)-$(NOW).txt;rm benchmarks/results/test-$(OFLAG)-$(NOW)-$$NANOSECONDS.txt
run-benchmark-%: $(OBJS) compile-% %.o
	mv $* benchmarks/bin/test-$(OFLAG)-$(NOW)
	-$(RM) benchmarks/bin/test-$(OFLAG)-recent
	ln benchmarks/bin/test-$(OFLAG)-$(NOW) benchmarks/bin/test-$(OFLAG)-recent
ifeq ($(RUN), 1)
	touch benchmarks/results/test-$(OFLAG)-$(NOW).txt
	$(TM_PRELOAD)targetlines=$$(expr $$(wc -l benchmarks/results/test-$(OFLAG)-$(NOW).txt | cut -f 1 -d ' ') + $(TRIALS));counter=1;while [ $$(wc -l benchmarks/results/test-$(OFLAG)-$(NOW).txt | cut -f 1 -d ' ') -lt $$targetlines ];do echo "[[ Running Trial $$counter ]]";$(PRERUN) timeout $(TIMEOUT) taskset -c 0-$$(expr $(NTHREADS) - 1) $(NUMACTL) benchmarks/bin/test-$(OFLAG)-$(NOW) $(POSTRUN);counter=$$(expr $$counter + 1);done
endif

.PHONY: run-%
run-%: $(OBJS) compile-% %.o
	@#$(PRERUN) $(NUMACTL) ./$* $(POSTRUN)
	@printf "run\\n\\t $(PRERUN) $(NUMACTL) ./$* $(POSTRUN)\\n\\n"

.PHONY: compile-%
compile-%: CCFLAGS += -DDIMENSIONS=$(DIMENSIONS)
compile-%: %.o
	$(TM_PRELOAD) $(CC) $(CFLAGS) $(CCFLAGS) $(OBJS) $*.o -o $*

%.o: CCFLAGS += -DDIMENSIONS=$(DIMENSIONS)
%.o: %.c
	$(CC) $(CFLAGS) $(CCFLAGS) -c $< -o $@ $(TESTFLAG)

.PHONY: clean
clean:
	-$(RM) $(OBJS) main.o test.o *.o
