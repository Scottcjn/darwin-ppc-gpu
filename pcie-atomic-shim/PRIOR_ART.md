# Prior-art claim: software emulation of device-issued PCIe atomics for ROCm HSA signaling on a no-atomics (PCIe Gen1) fabric

- Author: Scott Boudreaux / Elyan Labs
- Date: 2026-06-29
- Repo: https://github.com/Scottcjn/darwin-ppc-gpu/tree/main/pcie-atomic-shim
- Supersedes the first stamp (git history); re-signed after tri-brain review to scope the claim to the evidence.

## Claim (the method)
A method to run ROCm's HSA signal model on a host/fabric that lacks PCIe atomic
operations (e.g. PCIe Gen1, Power Mac G5): the GPU posts each signal
read-modify-write as a plain ordered write into a per-queue SPSC mailbox, and a
single host-side completer performs the actual RMW. Atomicity is provided by
serialization through one agent, so no hardware atomic instruction and no
device-issued PCIe AtomicOp is required. Integration is software-only at the
ROCr signal layer (hsa_signal_*); no GPU microcode change.

## What is demonstrated (off-target, x86-64)
The serialization protocol is bit-exact, while a DEFINED no-atomic-RMW model
(relaxed load then relaxed store) loses ~76% of concurrent updates. Emulated
signaling runs at ~21 Mops/s, orders of magnitude above any inference dispatch
rate, so the overhead amortizes to negligible for compute-heavy, signal-light
workloads.

## What is NOT yet demonstrated (identified, bounded)
- The big-endian wire format on the real target (this proof is x86-64 LE on both
  sides; the byteswap contract is encoded at le64_wire/le64_host as a no-op here).
- Full HSA acquire/release/wait semantics and payload visibility.
These are the real-hardware integration; the novel method and the protocol
correctness are what this claim covers.

## Artifact fingerprint (SHA-256)
- pcie_atomic_shim.c : 0e5b299811a32aa9691e6ad74b6b8f74b73b126fff9ada3136113be5a8fc6e19
- git commit         : 98b85245423898d5a446da0d3d3f328ef66edf91
