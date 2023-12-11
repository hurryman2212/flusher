# flusher

This is a kernel module for invalidating & flushing all cache/TLB globally via write-back.

## Usage

For flushing cache: Write "1" to /sys/kernel/cache/flush_cache `echo 1 | sudo tee /sys/kernel/cache/flush_cache`.

For flushing TLB: Write "1" to /sys/kernel/tlb/flush_tlb `echo 1 | sudo tee /sys/kernel/tlb/flush_tlb`.
