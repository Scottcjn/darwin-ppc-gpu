# pcie-atomic-shim: ROCm signaling on a fabric with no PCIe atomics

## The wall
ROCm's HSA signal model needs the GPU, as a PCIe requester, to issue atomic
read-modify-write to host memory (signal decrements, user-queue management). PCIe
Gen1 has no atomics, so a Power Mac G5 (or any pre-3.0 host) cannot run the
standard HSA path. AMD's own tooling flags this as a hard requirement.

## The substrate
The GPU never issues an atomic. It posts a request (a plain, ordered write the
Gen1 link CAN route) into a per-queue SPSC mailbox. A single host-side **completer**
drains the mailboxes and performs the actual RMW. Because exactly one agent ever
touches the signal, atomicity comes from **serialization**, not from a hardware
atomic instruction. The signal itself needs no atomic at all (single writer).

Integration is pure software: translate `hsa_signal_*` operations under ROCr into
shim calls. No GPU microcode change. The GPU keeps doing its plain EOP-fence
writes; the shim supplies the atomic *semantics* the HSA runtime expects.

## Proof (off-target, `make && ./shim`)
8 producers x 500k ops against a signal initialized to 4,000,000 (correct final = 0):

| mode | final signal | verdict | Mops/sec |
|------|-------------:|---------|---------:|
| NATIVE (Gen3+ PCIe atomics) | 0 | CORRECT | ~66 |
| NAIVE  (Gen1, defined no-atomic-RMW model) | ~3.0M | CORRUPT (~76% lost) | ~500 |
| SHIM   (Gen1 + this substrate) | 0 | CORRECT | ~20 |

The NAIVE row is the wall: without atomics the plain writes race and lose most
updates. The SHIM row restores exact correctness on a no-atomics fabric.

## Why the overhead is free here
~20 Mops/sec of emulated signaling is ~3x slower than native atomics and orders
of magnitude beyond any inference workload. A matmul dispatch signals once; even
10k dispatches/sec uses 0.05% of the shim's capacity. Signal-light workloads do
not care that signaling got more expensive.

## Scope of this proof (tri-brain hardened)
Off-target on x86-64, this proves the SERIALIZATION PROTOCOL is correct and cheap.
It does NOT prove (a) the big-endian wire format of the real target (both sides
here are little-endian x86; the byteswap contract is encoded at `le64_wire`/
`le64_host` but exercised as a no-op), nor (b) full HSA acquire/release/wait
semantics. The NAIVE control uses defined relaxed atomics (load then store, no RMW
atomicity), so its lost-update result is defined behavior, not UB.

## Honest boundary
This proves the signaling PROTOCOL is correct and cheap, off-target. The remaining
real-hardware work is (1) interposing it under ROCr's signal layer and (2) wiring
the producer side to the GPU's actual posted writes on the MI50. The protocol is
the part that was in doubt; it holds.

Built by Elyan Labs.
