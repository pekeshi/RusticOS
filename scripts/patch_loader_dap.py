#!/usr/bin/env python3
import sys, struct
if len(sys.argv) < 3:
    print("Usage: patch_loader_dap.py <loader.bin> <kernel_seek>", file=sys.stderr)
    sys.exit(2)
fn = sys.argv[1]
kernel_seek = int(sys.argv[2])
with open(fn, 'r+b') as f:
    data = f.read()
    marker = struct.pack('<Q', 0x1122334455667788)
    off = data.find(marker)
    if off == -1:
        # marker not found; nothing to do
        sys.exit(0)
    f.seek(off)
    f.write(struct.pack('<Q', kernel_seek))
