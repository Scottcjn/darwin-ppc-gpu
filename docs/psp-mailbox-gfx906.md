# Piece 3 spec: PSP mailbox firmware load (gfx906 / Vega 20, PSP v11)

Shape and the sequencing caveat came from AMD's ROCm AI assistant (2026-06-29).
The register offsets, register roles, and the command-buffer struct it gave were
WRONG in detail (hallucinated). Everything below is corrected against source:
`psp_v11_0.c`, `include/asic_reg/mp/mp_11_0_offset.h`, `psp_gfx_if.h`.

## Registers (VERIFIED, mp_11_0_offset.h). Address via RREG32_SOC15(MP0, 0, sym).
| Symbol | Offset | Role (from psp_v11_0.c) |
|--------|--------|--------------------------|
| mmMP0_SMN_C2PMSG_35 | 0x0063 | bootloader command + READY = bit 31 set (poll here, psp_v11_0.c:179) |
| mmMP0_SMN_C2PMSG_36 | 0x0064 | command-buffer address, written as (mc_addr >> 20) (psp_v11_0.c:225) |
| mmMP0_SMN_C2PMSG_81 | 0x0091 | sOS / boot status (sol_reg) -- is firmware already resident |
| mmMP0_SMN_C2PMSG_33 | 0x0061 | error status only (NOT the ready poll the AI claimed) |
| mmMP0_SMN_C2PMSG_64 | 0x0080 | GPCOM ring control (ring create/stop, psp_v11_0.c:299) |
| mmMP0_SMN_C2PMSG_101| 0x00A1 | ring control |

## Command buffer struct (VERIFIED, psp_gfx_if.h: struct psp_gfx_cmd_resp)
    +0  uint32 buf_size
    +4  uint32 buf_version   (= PSP_GFX_CMD_BUF_VERSION)
    +8  uint32 cmd_id        (cmd_id is at +8, NOT +0)
    +12 uint32 resp_buf_addr_lo
    +16 uint32 resp_buf_addr_hi
    +20 uint32 resp_offset
    +24 uint32 resp_buf_size
    +28 union  cmd           (command-specific)
All fields little-endian -> byteswap on PPC before writing (OSWriteLittleInt32).

## Bootloader sequence (VERIFIED, psp_v11_0.c)
1. wait_for_bootloader: poll C2PMSG_35 until bit 31 set.
2. Load sysdrv/SOS: WREG C2PMSG_36 = (blob_mc_addr >> 20); WREG C2PMSG_35 = cmd;
   wait_for_bootloader again.
3. SOS must be resident (C2PMSG_81 / sol_reg) before any GPCOM command will ACK.
4. Then ring_create (C2PMSG_64) and load remaining FW (ASD, MEC, RLC) via the ring.

## Keeper design decisions (sound, verified)
- VRAM-only command buffer + VRAM fence => PSP can sequence BEFORE GART (piece 3
  decouples from piece 2). Fence write-back to SYSTEM memory needs GART live.
- SOS is the gate: if C2PMSG_35 never goes ready / C2PMSG_81 never updates, the
  blob PA byteswap or the GPU-visible aperture is wrong.

## Lesson
A domain LLM gave correct protocol shape and a confidently wrong coordinate set.
Shape from the model, coordinates from the source. Always.
