all: coll_perf

.PHONY: coll_perf coll_perf_dbg

LDFLAGS=-llmpe -lmpe -lmatrix_mult
LIBMPE=$(LDFLAGS)

coll_perf: coll_perf.c
	mpicc -o $@ $< $(CFLAGS) -I$(INSTALL_DIR)/include -L$(INSTALL_DIR)/lib $(LIBMPE) -lm -lmpiwrap -ljson -g -O2

coll_perf_dbg: coll_perf.c
	mpicc -o coll_perf $< $(CFLAGS) -I$(INSTALL_DIR)/include -L$(INSTALL_DIR)/lib $(LIBMPE) -lm -lmpiwrap -ljson -g -O0 -DENABLE_DEBUG

clean:
	rm coll_perf
