# Piece 4 spec: GFX/compute ring bring-up (gfx906), verified against source

High-level sequence and the smoke-test idea came from AMD's ROCm AI assistant
(2026-06-29). Specifics verified/corrected against gfx_v9_0.c / soc15d.h.

## Load order (VERIFIED: rlc_resume precedes cp_resume in hw_init)
SOS -> ASD -> RLC -> MEC1/MEC2. RLC manages CU power-gating and wavefront
scheduling; without it the CP stalls. (gfx_v9_0_rlc_resume @3195 before
gfx_v9_0_cp_resume @3945, called in gfx_v9_0_hw_init @4026.)

## Ring registers (VERIFIED, gfx_v9_0.c:3396-3434)
- mmCP_RB0_BASE     = ring->gpu_addr >> 8   (so the ring is 256-byte aligned)
- mmCP_RB0_BASE_HI  = upper_32_bits(addr >> 8)
- mmCP_RB0_CNTL.RB_BUFSZ = order_base_2(ring_size / 8)   # log2(QWORDS), NOT dwords
  (the AI said log2(dwords); it is log2(ring_size/8). off by one bit.)
- mmCP_RB0_CNTL.RB_BLKSZ = RB_BUFSZ - 2

## Endianness (VERIFIED + REVISED): hardware, not software
gfx_v9_0.c:3409-3411 sets CP_RB0_CNTL.BUF_SWAP=1 under #ifdef __BIG_ENDIAN. The CP
byteswaps ring fetches in hardware, so amdgpu_ring_write's raw native store is
correct on BE WITH BUF_SWAP set. No per-dword cpu_to_le32 required.

COMPUTE PATH (our MVP) has its OWN hardware swap: gfx_v9_0.c:3633 sets
CP_HQD_PQ_CONTROL.ENDIAN_SWAP=1 under #ifdef __BIG_ENDIAN. So the compute
queue/MQD endianness is a hardware bit too -- set ENDIAN_SWAP on the HQD and the CP
swaps. gfx_v9_0.c has FOUR __BIG_ENDIAN swap sites total (gfx ring BUF_SWAP,
compute HQD ENDIAN_SWAP, and two IB/RLC endian-mode fields) -- AMD's GFX engine
carries comprehensive latent BE support.

## First PM4 smoke test (sound)
PACKET3(PACKET3_NOP, ...) then PACKET3(PACKET3_WRITE_DATA=0x37, ...) writing a
known value to a known VRAM/GTT location, bump WPTR via doorbell, then poll that
location from the host. If the value lands, the ring is live. Everything else
(dispatch, fences, IB chaining) builds on that one confirmation.

Verified against torvalds/linux mainline amdgpu (2026-06).
