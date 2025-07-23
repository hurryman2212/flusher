# flusher

This is the kernel module for flushing & subsequently invalidating all caches/NPT/TLB on all CPU(s) via write-back.

For EPT/NPT: Only the corresponding interface for the running platform will be created.

## Usage

For flushing cache: Write "1" to /sys/kernel/cache/flush via `echo 1 | sudo tee /sys/kernel/cache/flush`.

For flushing EPT TLB (for Intel platform): Write "1" to /sys/kernel/npt/flush via `echo 1 | sudo tee /sys/kernel/ept/flush`.
For flushing NPT TLB (for AMD platform): Write "1" to /sys/kernel/npt/flush via `echo 1 | sudo tee /sys/kernel/npt/flush`.

For flushing TLB: Write "1" to /sys/kernel/tlb/flush via `echo 1 | sudo tee /sys/kernel/tlb/flush`.
