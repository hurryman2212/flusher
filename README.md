# flusher

This is the kernel module for flushing & subsequently invalidating all caches/NPT/TLB on all CPU(s) via write-back.

For NPT: Only Intel EPT is currently supported (if not supported, there will be no interface created).

## Usage

For flushing cache: Write "1" to /sys/kernel/cache/flush via `echo 1 | sudo tee /sys/kernel/cache/flush`.

For flushing NPT: Write "1" to /sys/kernel/npt/flush via `echo 1 | sudo tee /sys/kernel/npt/flush`.

For flushing TLB: Write "1" to /sys/kernel/tlb/flush via `echo 1 | sudo tee /sys/kernel/tlb/flush`.
