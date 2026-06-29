CC ?= cc
CFLAGS ?= -std=gnu11 -I. -Wall -Wno-unused-function -Wno-format-zero-length -Wno-pointer-sign
atom_test: atom.c atom_standalone_test.c darwin_compat.h
	$(CC) $(CFLAGS) atom.c atom_standalone_test.c -o $@
clean:
	rm -f atom_test
