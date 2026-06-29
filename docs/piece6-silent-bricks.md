# Pre-flight: the silent bricks of a bare-metal Vega20 compute bring-up

Hard-won init that does NOT show up as a single packet and bricks you silently.
Source: AMD ROCm AI assistant (2026-06-29), VERIFIED against gfx_v9_0.c / soc15d.h.
Register names corrected to source where the assistant approximated.

| Item | Symptom if skipped | Verified | Do this |
|------|--------------------|----------|---------|
| **HQD dequeue before re-program** | undefined behavior on re-init | EXACT (gfx_v9_0.c:3708-3819) | Poll `mmCP_HQD_ACTIVE` until bit0=0 (write `CP_HQD_ACTIVE=0` to force), THEN write `CP_HQD_PQ_BASE` and the rest of the MQD. You re-init after every failed attempt -- this is the one that bites most. |
| **Golden registers (Vega20)** | silent wrong results / data corruption (no hang) | YES (`golden_settings_gc_9_0_vg20[]` @676/722) | Apply the vg20 golden table BEFORE the smoke test. Looks like magic numbers, isn't optional. |
| **GFXOFF disable** | ring "disappears" mid-run | concept (PP_GFXOFF_MASK @1422) | Keep GFX powered for bare-metal; do not let GFXOFF gate the block. |
| **CU mask = all-on** | DISPATCH_DIRECT completes instantly, ZERO output, no hang | concept YES, name corrected | Per-queue mask is `mqd->dynamic_cu_mask = 0xFFFFFFFF` (gfx_v9_0.c:3868), NOT `mmCC_GC_SHADER_ARRAY_CONFIG` (that's the global fused-off harvest read). Ensure dynamic_cu_mask != 0. |
| **TC/L1/L2 flush before 1st dispatch** | first kernel reads garbage from stale POST cache | YES (`PACKET3_ACQUIRE_MEM`=0x58, soc15d.h:392) | Emit `ACQUIRE_MEM` (0x58) with TC/TCL1/TC_WB action-enable bits (CP_COHER_CNTL flags @403-411) before the first `DISPATCH_DIRECT`. |
| **SMU driver-ready** | CP stalls, no ring activity | smu_v11_0.c | Send the SMU driver-ready handshake (MP1 C2PMSG) to ungate clocks before the CP runs. See piece 3 PSP/SMU. |

## The two that hurt most
1. Golden registers -> silent corruption (you'd debug the wrong layer for days).
2. HQD-dequeue-before-reinit -> you WILL re-init after the first failures, and a live
   HQD makes the second attempt behave randomly.

Verified against torvalds/linux mainline amdgpu (2026-06).
