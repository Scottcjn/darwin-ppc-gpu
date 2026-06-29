# darwin-ppc-gpu

Modern AMD (and, planned, NVIDIA) GPU bring-up for **PowerPC Mac OS X** (Tiger /
Leopard) as native **Darwin/XNU IOKit kernel extensions**. Target: drive an
**AMD Instinct MI50** (Vega 20 / gfx906) as an LLM compute accelerator on a
Power Mac G5 (ppc64, **big-endian**), so a 2005 Mac hosts a datacenter card.

The card POSTs without x86 because AMD's GPU init is **ATOM BIOS** (an
architecture-independent bytecode interpreter), not x86 VBIOS code. Steady-state
inference is HBM-bound, not bus-bound: load the model into VRAM once, then the
host just builds command buffers and rings a doorbell.

## The driver in 5 pieces

| # | Piece | Status |
|---|-------|--------|
| 1 | **ATOM BIOS POST** (card init, x86-free) | **Proven Linux-free (off-target), tri-brain hardened** |
| 2 | HBM controller + GART/GPUVM | planned |
| 3 | PSP/SMU firmware load | planned |
| 4 | One compute queue (ring + doorbell) | planned |
| 5 | PM4 dispatch + bulk weight upload | planned |

NVIDIA backport (nouveau lineage) tracked as a parallel effort.

## Piece 1 (this repo)

`piece1-atom-post/` extracts AMD's MIT-licensed ATOM interpreter (`atom.c`) from
mainline Linux and proves it builds and runs **free of Linux/DRM** behind a small
shim (`darwin_compat.h`). On the real kext that shim maps to IOKit
(`IOMalloc`/`IOLog`/`IODelay`/`IOLock`) and the `card_info` MMIO callbacks do the
big-endian byteswap via `OSReadLittleInt32`/`OSWriteLittleInt32`.

```sh
cd piece1-atom-post && make && ./atom_test
```

### Why big-endian is mostly free here
The interpreter's table reads (`get_u16`/`get_u32`) are assembled from individual
byte reads with explicit shifts, so they yield the correct little-endian value on
**any** host endianness. The one genuine BE work item is the packed-struct field
reads in `amdgpu_atom_parse` (the VBIOS ROM header), which need byteswap on a
big-endian host. Everything else is endian-clean by construction.

See `docs/five-piece-map.md` for the full plan and the upstream source map.

Built and verified by Elyan Labs on real PowerPC hardware.
