#!/usr/bin/env python
import sys
import os
from pathlib import Path

while True:
    r = sys.stdin.buffer.read(1)
    if len(r) == 0:
        break

(Path(os.environ["TMPDIR"]) / "xnmp-write-on-close").touch()
