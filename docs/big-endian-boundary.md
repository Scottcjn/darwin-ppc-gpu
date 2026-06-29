# The big-endian boundary: the whole port as ~5 primitives

The G5 is ppc64 big-endian; the MI50 expects little-endian at every host<->device
data crossing. The key structural finding, verified across all five pieces against
mainline source: amdgpu does NOT scatter endianness through its logic. Every
crossing centralizes at a small, countable set of boundary primitives. Port those
with a byteswap and the logic above them is endian-correct for free.

| Piece | Boundary primitive (source) | Status | BE fix on Darwin/PPC |
|-------|------------------------------|--------|----------------------|
| 1 POST | ATOM `get_u8/16/32` (atom-bits.h); MMIO via `card_info` reg/mc/pll callbacks | VERIFIED | reads are byte-assembled = already BE-safe; MMIO callbacks use `OSReadLittleInt32`/`OSWriteLittleInt32` |
| 2 GMC/GART/VM | `amdgpu_gmc_set_pte_pde` -> `writeq` (amdgpu_gmc.c:175); CPU PTE path `amdgpu_vm_cpu_update` calls the same | VERIFIED | one primitive: `writeq` -> `OSWriteLittleInt64`. Value math is endian-neutral. Covers GART + GPUVM CPU path. SDMA PTE path separate (not in MVP). |
| 3 PSP | C2PMSG MMIO regs (mp_11_0_offset.h); `psp_gfx_cmd_resp` struct fields | VERIFIED | MMIO accessors byteswap; byteswap the cmd-buffer struct fields before write |
| 4 Ring/PM4 | `amdgpu_ring_write` raw uint32 store (amdgpu_ring.h:496) + `amdgpu_ring_write_multiple` | VERIFIED | one chokepoint: `cpu_to_le32` (OSSwapHostToLittleInt32) per dword at the store |
| 5 WPTR/RPTR | doorbell MMIO + memory shadow via per-asic `get_wptr`/`set_wptr` (gfx_v9_0) | MECHANISM CONFIRMED | doorbell via MMIO accessor; byteswap the in-memory wptr/rptr shadow. Per-asic callback sites to enumerate. |

## Why this matters
The BE port is bounded. It is "fix the ~5 boundary primitives," not "audit 10,000
sites." This is the same reason AMD's 2008 ATOM interpreter already ran on PowerPC
Macs: keep endianness at the hardware boundary, and portability follows.

Verified against torvalds/linux mainline amdgpu (2026-06).
