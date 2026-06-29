# Piece 1 review (tri-brain: Codex + Grok)

Converged in 2 loops. Findings, all in the shim (atom.c logic unchanged):

- **scnprintf**: must return chars actually stored capped at size-1 (callers do
  `off += scnprintf`), not snprintf's would-be length. Fixed.
- **jiffies / jiffies_to_msecs**: jiffies is now monotonic milliseconds so the
  ATOM command-table loop-guard (20s timeout) computes correctly. Fixed.
- **strscpy**: returns -E2BIG on truncation per the Linux contract. Fixed.
- **kmalloc_array**: unused in atom.c; removed.
- Grok independently verified `cpu_to_le32` + `get_unaligned_le*` round-trip
  correctly on both LE and BE.

Remaining warnings are upstream atom.c `-Wpointer-sign` (char vs uint8_t),
suppressed the same way the kernel build does.
