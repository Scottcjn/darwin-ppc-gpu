# Prior-art claim: software emulation of device-issued PCIe atomics for ROCm HSA signaling on a no-atomics (PCIe Gen1) fabric

- Author: Scott Boudreaux / Elyan Labs
- Date: 2026-06-29
- Repo: https://github.com/Scottcjn/darwin-ppc-gpu/tree/main/pcie-atomic-shim
- Git commit: 55d763d563b859acd1ffc63e1c2eb3a92dd31f18

## Claim
A method to run ROCm's HSA signal model on a host/fabric that lacks PCIe atomic
operations (e.g. PCIe Gen1, Power Mac G5), by having the GPU post each signal
read-modify-write as a plain ordered write into a per-queue SPSC mailbox, and a
single host-side completer perform the actual RMW. Atomicity is provided by
serialization through one agent, so no hardware atomic instruction and no
device-issued PCIe AtomicOp is required. Integration is software-only at the
ROCr signal layer (hsa_signal_*); no GPU microcode change.

Demonstrated off-target: a naive no-atomics path loses ~86% of concurrent signal
updates, while this substrate is bit-exact, at a signaling throughput (~20 Mops/s)
orders of magnitude above any inference dispatch rate, so the overhead amortizes
to negligible for compute-heavy, signal-light workloads.

## Artifact fingerprints (SHA-256)
- pcie_atomic_shim.c : e9192bfdc9a4ff4a804379396c2a1a1021ab10cc3ab6ab8ce0df8bfd4a4ed5e1
- README.md          : d26a2f50b003d32886fcb4997d311eaf0574e1b9fe14d4b34b75476b6d0f65b5
