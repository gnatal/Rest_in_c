# Builds the TFB app against the CExpress engine copied into ./engine (Makefile + lib/) by ../run.sh.
CC ?= gcc
CFLAGS = -Wall -Wextra -std=c11 -O2 -D_GNU_SOURCE -Iengine/lib -I$(shell pg_config --includedir)
LDLIBS = -Lengine/build/lib -lcexpress -L$(shell pg_config --libdir) -lpq

tfb-cexpress: src/main.c src/db.c src/db.h src/tfb_types.h engine/build/lib/libcexpress.a
	$(CC) $(CFLAGS) -o $@ src/main.c src/db.c $(LDLIBS)

# epoll build: the engine's io_uring backend is only a readiness poller and measured slower.
engine/build/lib/libcexpress.a:
	$(MAKE) -C engine NO_URING=1

clean:
	rm -f tfb-cexpress
	$(MAKE) -C engine clean
