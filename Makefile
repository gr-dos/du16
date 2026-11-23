# Build du.exe for 16-bit MS-DOS with Open Watcom and libc16.
#   make             fetch the pinned libc16 release into build/libc16,
#                    then build du.exe
#   make LIBC16_ROOT=../libc16
#                    use a libc16 checkout built with its make
#   make test        host unit tests, then DOS tests under DOSBox-X
#   make test-grdos  DOS tests on GR-DOS itself, booted in QEMU
#
# du.exe is compiled and linked against libc16 only (its ABI is in
# libc16's docs/ABI.md), with the Open Watcom toolchain libc16 was built
# with: the libc16 tree's tools/fetch-watcom fetches it into build/tools.

LIBC16_ROOT ?= build/libc16
# A release tree, or a checkout, whose make stages the same layout in
# build/stage.
LIBC16_DIR  := $(if $(wildcard $(LIBC16_ROOT)/build/stage/VERSION),$(LIBC16_ROOT)/build/stage,$(LIBC16_ROOT))
LIBC16      := $(LIBC16_DIR)/lib/LIBC16.LIB
TOOLS_ENV   := build/tools.env
WATCOM_ENV   = . ./$(TOOLS_ENV) && PATH=$$WATCOM/binl64:$$PATH

VERSION := $(shell cat VERSION)

CC     = wcc
CFLAGS = -q -zq -zl -zld -x -ms -0 -os -s -bt=dos -wx -we \
         -I$(LIBC16_DIR)/include '-DVERSION="$(VERSION)"'

# C99 for unsigned long long, which the 64-bit totals need
HOSTCC     ?= cc
HOSTCFLAGS ?= -std=c99 -pedantic -Wall -Wextra -Werror -O2

SRCS = src/main.c src/du.c src/options.c src/format.c src/dosfs.c src/out.c
HDRS = src/du.h src/options.h src/format.h src/dosfs.h src/out.h

OBJS = $(patsubst src/%.c,build/dos/%.obj,$(SRCS))

UNIT = build/test_format build/test_options build/test_du

du.exe: $(OBJS) $(LIBC16) $(TOOLS_ENV)
	$(WATCOM_ENV) && wlink option quiet, nodefaultlibs, map=build/du.map \
	  format dos name $@ $(foreach o,$(OBJS),file $(o)) library $(LIBC16)

build/dos/%.obj: src/%.c $(HDRS) VERSION $(LIBC16) $(TOOLS_ENV)
	@mkdir -p build/dos
	$(WATCOM_ENV) && $(CC) $(CFLAGS) -fo=$@ $<

# The pinned libc16 release (scripts/fetch-libc16). A LIBC16_ROOT given
# on the command line must already be built.
ifeq ($(LIBC16_ROOT),build/libc16)
$(LIBC16): scripts/fetch-libc16
	scripts/fetch-libc16 $(LIBC16_ROOT)
endif

# The Open Watcom snapshot libc16 names, as shell assignments.
$(TOOLS_ENV): $(LIBC16)
	mkdir -p build
	watcom=$$(sh $(LIBC16_DIR)/tools/fetch-watcom $(CURDIR)/build/tools) \
	  && printf 'WATCOM=%s\nexport WATCOM\n' "$$watcom" > $@

build/test_format: tests/test_format.c tests/unit.h src/format.c src/format.h \
		src/options.h
	@mkdir -p build
	$(HOSTCC) $(HOSTCFLAGS) -Isrc -o $@ tests/test_format.c src/format.c

build/test_options: tests/test_options.c tests/unit.h src/options.c src/options.h \
		src/out.c src/out.h
	@mkdir -p build
	$(HOSTCC) $(HOSTCFLAGS) -Isrc -o $@ tests/test_options.c src/options.c \
		src/out.c

build/test_du: tests/test_du.c tests/unit.h src/du.c src/du.h \
		src/options.c src/options.h src/format.c src/format.h \
		src/out.c src/out.h
	@mkdir -p build
	$(HOSTCC) $(HOSTCFLAGS) -Isrc -o $@ tests/test_du.c src/du.c \
		src/options.c src/format.c src/out.c

unit: $(UNIT)
	@for t in $(UNIT); do echo "$$t"; ./$$t || exit 1; done

# DOS test of the critical-error handling in src/dosfs.c
build/int24.exe: build/dos/int24.obj build/dos/dosfs.obj $(LIBC16) $(TOOLS_ENV)
	$(WATCOM_ENV) && wlink option quiet, nodefaultlibs format dos name $@ \
	  file build/dos/int24.obj file build/dos/dosfs.obj library $(LIBC16)

build/dos/int24.obj: tests/int24.c $(HDRS) $(LIBC16) $(TOOLS_ENV)
	@mkdir -p build/dos
	$(WATCOM_ENV) && $(CC) $(CFLAGS) -Isrc -fo=$@ tests/int24.c

dostest: du.exe build/int24.exe
	sh tests/run.sh $(CURDIR)/du.exe $(CURDIR)/build/int24.exe

test: unit dostest

# Not part of make test: it downloads a GR-DOS release and needs QEMU.
test-grdos: du.exe build/int24.exe
	sh tests/grdos.sh $(CURDIR)/du.exe $(CURDIR)/build/int24.exe

# Keeps the fetched libc16, Open Watcom and GR-DOS (build/libc16,
# build/tools, build/grdos).
clean:
	rm -rf *.o *.obj *.err du.exe \
	  $(filter-out build/libc16 build/libc16.zip build/tools build/grdos,$(wildcard build/*))

.PHONY: unit dostest test test-grdos clean
