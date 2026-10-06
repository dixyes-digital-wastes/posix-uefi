# Tests for the parts of the library that do not need the firmware.
#
# strtol is pure code that decides whether a number in a configuration file is
# a number, so it can be checked on the host in seconds. Anything that touches
# the firmware has to be exercised by booting, which the project that consumes
# this fork does.

ifeq ($(origin CC),default)
CC := clang
endif

UEFI := ../uefi

.PHONY: all check clean
all: test_strtol

# stdlib.c is linked whole, so the test supplies stubs for the firmware globals
# that its unreached functions reference. The names under test are renamed so
# they cannot collide with the host libc, and -Wno-invalid-noreturn silences
# the host declarations of abort() and exit(), which this library never
# returns from.
test_strtol: test_strtol.c $(UEFI)/stdlib.c
	$(CC) -std=gnu23 -O2 -Wall -Wextra \
	      -Wno-incompatible-library-redeclaration -Wno-invalid-noreturn \
	      -Dstrtol=usStrtol -Dstrtoll=usStrtoll -Dstrtoull=usStrtoull \
	      -I$(UEFI) $^ -o $@

check: all
	./test_strtol

clean:
	rm -f test_strtol
