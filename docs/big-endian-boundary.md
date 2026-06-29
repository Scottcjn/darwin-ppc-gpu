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
| 4 Ring/PM4 | `amdgpu_ring_write` raw uint32 store (amdgpu_ring.h:496); CP_RB0_CNTL.BUF_SWAP (gfx_v9_0.c:3410) | VERIFIED + REVISED | HARDWARE: set `CP_RB0_CNTL.BUF_SWAP=1` (amdgpu does this `#ifdef __BIG_ENDIAN`) and the CP byteswaps ring fetches itself -> the raw native store is correct, NO per-dword `cpu_to_le32`. Software swap only if BUF_SWAP is not used. |
| 5 WPTR/RPTR | `gfx_v9_0_ring_set_wptr_compute`:5692 -- `atomic64_set(wptr_cpu_addr)` shadow + `WDOORBELL64` | **AMD-UNVERIFIED** | amdgpu has `/* XXX check if swapping is necessary on BE */` at gfx_v9_0.c:5696. Doorbell = MMIO (LE accessor, fine). The WPTR memory shadow the CP polls (`cp_hqd_pq_wptr_poll_addr`) likely needs `cpu_to_le64` on BE but AMD never confirmed it. THE empirical unknown -- only the G5 answers it. |

## Why this matters
The BE port is bounded. It is "fix the ~5 boundary primitives," not "audit 10,000
sites." This is the same reason AMD's 2008 ATOM interpreter already ran on PowerPC
Macs: keep endianness at the hardware boundary, and portability follows.

## Latent big-endian support in amdgpu
gfx_v9_0.c guards `CP_RB0_CNTL.BUF_SWAP` with `#ifdef __BIG_ENDIAN` -- AMD left a
hardware BE path in the ring code. Worth grepping the gfx/sdma/gmc sources for
other `#ifdef __BIG_ENDIAN` blocks and hardware *_SWAP register fields before
writing software byteswaps; some of the BE work is already done and just needs the
build to define big-endian. The one exception is the compute WPTR shadow
(gfx_v9_0.c:5696) where amdgpu's own `/* XXX check if swapping is necessary on BE */`
admits it was never verified -- that is the single empirical unknown for the port.

Verified against torvalds/linux mainline amdgpu (2026-06).
