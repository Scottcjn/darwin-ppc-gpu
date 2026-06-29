# MI50 (gfx906) compute KEXT: the 5-piece map

Upstream sources are gfx906 / SOC15 / GFX9 generation.

| # | Piece | Upstream source | Big-endian work |
|---|-------|-----------------|-----------------|
| 1 | ATOM POST | `atom.c`, `amdgpu_atombios.c` | byteswap the packed VBIOS-header struct reads; table reads already BE-safe |
| 2 | HBM + GART/VM | `gmc_v9_0.c`, `gfxhub_v1_0.c`, `mmhub_v1_0.c`, `amdgpu_gart.c`, `amdgpu_vm_cpu.c` | VERIFIED: all PTE/PDE writes funnel through `amdgpu_gmc_set_pte_pde` -> `writeq`. Replicate `writeq` as `OSWriteLittleInt64`; value math is endian-neutral. (SDMA PTE path is separate, not in MVP.) IOKit coupling is the real cost, not endianness. |
| 3 | PSP/SMU firmware | `psp_v11_0.c`, `amdgpu_psp.c`; power via legacy powerplay `vega20_smumgr.c` | PSP ring descriptor endianness; load blobs from kext bundle |
| 4 | One compute queue | `gfx_v9_0.c`, `amdgpu_ring.c`, `vega20_ih.c` | ring buffer + WPTR/RPTR endianness; IOKit doorbell |
| 5 | Compute queue + PM4 dispatch | `gfx_v9_0_mqd_init` (MQD/CP_HQD_PQ_*), `SET_SH_REG` 0x76 + `DISPATCH_DIRECT` 0x15 | VERIFIED (docs/piece5-compute-dispatch.md). Ring data endianness = hardware ENDIAN_SWAP; MQD MMIO = LE accessor; only WPTR shadow unknown. |

Bus note: PCIe Gen1 is fine. Weights cross once at load (HBM-bound after).
Power note: a bare ATX PSU + 24-pin jumper drives the card's 8+8-pin aux; the
G5's internal PCIe slot carries data, so no OcuLink is required.
