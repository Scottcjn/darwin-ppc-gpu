# Piece 5 spec: compute queue (MQD) + kernel dispatch (gfx906), verified

Completes the compute path: POST -> GMC/GART -> PSP -> ring -> THIS (queue + launch).
Verified against gfx_v9_0.c and soc15d.h.

## Compute queue = an MQD (gfx_v9_0_mqd_init @3549)
Fill a Memory/Hardware Queue Descriptor, write its fields to the CP_HQD_PQ_*
registers via WREG32_SOC15 (MMIO -> LE accessor), then set it active:

| MQD field | Source | Meaning / endianness |
|-----------|--------|----------------------|
| cp_hqd_pq_base_lo/hi | 3622-3624 | ring gpu_addr >> 8 (256-byte aligned) |
| cp_hqd_pq_control | 3629/3639 | size = `order_base_2(ring_size/4) - 1` (dwords, NOT the gfx ring's /8); + ENDIAN_SWAP=1 `#ifdef __BIG_ENDIAN` (3633) -> hardware ring swap |
| cp_hqd_pq_doorbell_control | 3591-3603 | DOORBELL_OFFSET(index) + DOORBELL_EN |
| cp_hqd_pq_wptr_poll_addr_lo/hi | 3649-3650 | WPTR shadow the CP polls -- **the BE-unknown (gfx_v9_0.c:5696 XXX)** |
| cp_hqd_pq_rptr_report_addr_lo/hi | 3643-3644 | CP writes RPTR back here -- LE (read with le32_to_cpu) |
| cp_hqd_active | 3680 | = 1 activates the queue |

## Kernel dispatch = two PM4 packets (soc15d.h)
1. PACKET3_SET_SH_REG (0x76, soc15d.h:435): load the compute shader config --
   COMPUTE_PGM_LO/HI (shader address), COMPUTE_PGM_RSRC1/2 (VGPR/SGPR setup),
   COMPUTE_USER_DATA_* (kernel-arg pointer), COMPUTE_NUM_THREAD_X/Y/Z.
2. PACKET3_DISPATCH_DIRECT (0x15, soc15d.h:92): grid dim x/y/z + dispatch
   initiator -> launches the kernel. (DISPATCH_INDIRECT 0x16 reads dims from mem.)

Emit both via amdgpu_ring_write (native dwords; BUF_SWAP/ENDIAN_SWAP handle BE),
bump WPTR (doorbell + shadow), poll the result/fence (LE, OSReadLittleInt32).

## Endianness summary for this piece
- MQD register writes: MMIO LE accessor (fine).
- Ring/IB data (the dispatch packets): hardware ENDIAN_SWAP (fine).
- RPTR report memory: LE contract (read le32_to_cpu) (fine).
- WPTR poll shadow: the one empirical unknown (see big-endian-boundary.md piece 5).

Verified against torvalds/linux mainline amdgpu (2026-06).
