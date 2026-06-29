# We ran your gfx906 ATOM BIOS on a big-endian PowerPC Mac, and `atom.c` didn't even flinch

We're bringing up an **Instinct MI50** (Vega 20, gfx906) as an LLM compute
accelerator on a **2005 Power Mac G5**: ppc64, **big-endian**, Mac OS X 10.5
Leopard, as a native Darwin/XNU IOKit KEXT. Your datacenter card, our 21-year-old
host. Step one was card POST, which meant your ATOM BIOS interpreter.

So first, a love letter. We pulled `atom.c` from mainline, retargeted **only its
include block** (Linux/DRM/amdgpu headers to a ~120-line compat shim), and it
compiled and ran with **zero changes to the interpreter logic**. It parsed a
VBIOS image, hit the magic check, returned cleanly. Off-target, no kernel, no
DRM. Code that portable is not an accident.

Then the part that made us grin. The reason a *modern* card POSTs on PowerPC at
all is that AMD init is ATOM bytecode, not x86 VBIOS, so there's nothing x86 to
run. And the interpreter is **big-endian-safe by construction**:

```c
static inline uint16_t get_u16(void *bios, int ptr){
    return get_u8(bios, ptr) | (((uint16_t)get_u8(bios, ptr+1)) << 8);
}
```

Byte reads assembled with explicit shifts yield the correct little-endian value
on *any* host endianness. Stanislaw Skowronek wrote that in 2008, which is
exactly why ATOM ran on PowerPC Macs and SPARC back in the day. We just showed
up in 2026 and collected on it.

A few notes for the curious:

- **MI50 over MI100 on purpose.** Vega 20 POSTs via the architecture-independent
  ATOM interpreter. CDNA (gfx908) trades that for GC IP-discovery plus PSP
  secure-boot and drops the display engine, so for an x86-free big-endian host,
  gfx906 is the open sweet spot.
- **The bus doesn't matter.** The G5's PCIe is Gen1. Doesn't care: weights load
  into 16GB HBM2 once, then it's HBM-bound. The Mac builds a command buffer and
  rings a doorbell. A PPC970 rings a doorbell just fine.
- **The one honest BE seam.** The byte-assembled table reads are clean, but
  `amdgpu_atom_parse` casts the VBIOS straight to packed structs
  (`_ATOM_ROM_HEADER` and friends), and *those* `USHORT`/`ULONG` field reads need
  byteswap on BE. That's the real work, and it's tiny.

It's MIT, it's public, and the rest of the driver (GMC/GART, PSP, compute ring,
PM4) is next. Your code travels well, even to places it was never meant to go.

Built by Elyan Labs on real PowerPC hardware.
