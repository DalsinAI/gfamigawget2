"""The test site tests/host-wget2.sh downloads: pages linking each other,
a style sheet with a url() link, and a 1 MB file with a fixed date.
MIT, Copyright (c) 2026 AmiCradle Team."""
import os
import random
import sys

root = sys.argv[1]
os.makedirs(os.path.join(root, "sub"), exist_ok=True)


def put(name, data):
    with open(os.path.join(root, name), "wb") as f:
        f.write(data)


put("index.html", b"""<!DOCTYPE html>
<html><head><title>GFOS Wget2 test</title><link rel="stylesheet" href="style.css"></head>
<body><p>Test site.</p><a href="sub/page2.html">page 2</a> <a href="sub/big.bin">big</a></body></html>
""")
put("style.css", b"body { background: url(sub/from-css.png) }\n")
put("sub/page2.html", b"""<!DOCTYPE html>
<html><body><a href="../index.html">back</a></body></html>
""")
put("sub/from-css.png", bytes(range(256)) * 4)
rng = random.Random(1985)
put("sub/big.bin", bytes(rng.getrandbits(8) for _ in range(1 << 20)))
# 1 Jan 2024 12:00:00 UTC: -N must put this date on the copy
os.utime(os.path.join(root, "sub/big.bin"), (1704110400, 1704110400))
