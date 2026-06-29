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
| 1 | **ATOM BIOS POST** (card init, x86-free) | **Proven Linux-free (off-target), tri-brain hardened** + IOKit kext skeleton |
| 2 | HBM controller + GART/GPUVM | mapped + verified (`docs/`) |
| 3 | PSP/SMU firmware load | mapped + verified (`docs/psp-mailbox-gfx906.md`) |
| 4 | Compute queue (ring + doorbell) | mapped + verified (`docs/piece4-ring-bringup.md`) |
| 5 | Compute MQD + PM4 dispatch | mapped + verified (`docs/piece5-compute-dispatch.md`) |

Every piece is traced against mainline amdgpu. The entire compute path (POST to
kernel launch) is source-verified; one BE detail (the WPTR shadow byteswap) is the
single thing only the hardware resolves.

NVIDIA backport (nouveau lineage) tracked as a parallel effort.

The `pcie-atomic-shim/` directory answers the PCIe-atomics question for Gen1 hosts: ROCm HSA signaling emulated via a host-mediated completer (atomicity by serialization), proven correct off-target.

## Hardware bring-up -- field guide (read at the G5)

Order of operations for first power-on, with the doc for each step:

1. **Load the kext** -- `piece1-kext/` (ElyanMI50): match the MI50, map BARs, dump
   the VBIOS. First milestone: `ioreg` shows it attached and the log prints `aa55`.
2. **POST the card** -- run the ATOM interpreter (`piece1-atom-post/`) on the VBIOS.
3. **GMC/GART** -- `docs/big-endian-boundary.md` (piece 2): `writeq` -> `OSWriteLittleInt64`.
4. **PSP firmware** -- `docs/psp-mailbox-gfx906.md`: SOS -> ASD -> RLC -> MEC.
5. **Ring + compute queue** -- `docs/piece4-ring-bringup.md` + `docs/piece5-compute-dispatch.md`:
   set `BUF_SWAP`/`ENDIAN_SWAP` (`#ifdef __BIG_ENDIAN`), program the MQD, doorbell.
6. **Smoke test** -- PM4 `NOP` + `WRITE_DATA` (0x37) of `0xDEADBEEF` to VRAM, poll
   with `OSReadLittleInt32`. If it lands, the ring is live.

**Before the smoke test, read `docs/piece6-silent-bricks.md`** -- the init that
fails silently (golden registers = data corruption, live-HQD-on-reinit = random
behavior, CU mask, ACQUIRE_MEM flush, GFXOFF, SMU driver-ready). Every item
source-verified.

**The one unknown:** the WPTR poll shadow (`gfx_v9_0.c:5696` has amdgpu's own
`/* XXX check if swapping is necessary on BE */`). Best guess: write it with
`cpu_to_le64`. Watch this on first ring kick.

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
