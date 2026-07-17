# Host test for the MeshCore .mota in-place apply — runs the REAL bootloader apply (ota_delta.c) over a
# simulated flash, NO hardware. See README.md.
#
#   make check                                  # apply the committed vector, assert success
#   ./apply_sim <base.img> <delta.mota> <new.img>   # debug any scenario (e.g. real firmware images)

CC      ?= gcc
CFLAGS  ?= -std=c11 -O2 -Wall -Wextra -Wno-unused-parameter
SRC      = ../src
INC      = -I $(SRC) -I $(SRC)/detools

apply_sim: apply_sim.c $(SRC)/ota_delta.c $(SRC)/detools/detools.c $(SRC)/sha256.c \
           $(SRC)/ota_delta.h $(SRC)/ota_layout.h
	$(CC) $(CFLAGS) -DOTA_DELTA_HOST_TEST $(INC) apply_sim.c $(SRC)/detools/detools.c $(SRC)/sha256.c -o $@

# Regression test for the -flto flash-readback aliasing bug (positive + negative behaviour + source guard).
readback_test: readback_test.c $(SRC)/ota_delta.c $(SRC)/detools/detools.c $(SRC)/sha256.c \
               $(SRC)/ota_delta.h $(SRC)/ota_layout.h
	$(CC) $(CFLAGS) -DOTA_DELTA_HOST_TEST $(INC) readback_test.c $(SRC)/detools/detools.c $(SRC)/sha256.c -o $@

check: apply_sim readback_test
	./apply_sim vectors/base.img vectors/delta.mota vectors/new.img
	@echo
	./readback_test

clean:
	rm -f apply_sim readback_test

.PHONY: check clean
