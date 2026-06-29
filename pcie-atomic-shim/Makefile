CC ?= cc
CFLAGS ?= -O2 -std=gnu11 -pthread
shim: pcie_atomic_shim.c
	$(CC) $(CFLAGS) pcie_atomic_shim.c -o $@
clean:
	rm -f shim
