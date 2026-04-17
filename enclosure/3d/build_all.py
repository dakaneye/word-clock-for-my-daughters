"""Regenerate every STL in enclosure/3d/out/ by running each part script.

Run from repo root:
    enclosure/3d/.venv/bin/python enclosure/3d/build_all.py

Add new parts by dropping a sibling `.py` in this directory that writes
its STL to `out/`. The loop below picks them up by filename pattern
`*_*.py` (skips this file and __init__.py).
"""
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parent
VENV_PY = ROOT / ".venv" / "bin" / "python"

# Parts are any .py in this dir other than this script, build_all itself,
# and dunder files. Use the venv python so build123d is on the path.
scripts = sorted(
    p for p in ROOT.glob("*.py")
    if p.name not in {"build_all.py", "__init__.py"}
)

if not scripts:
    print("no part scripts found in", ROOT)
    sys.exit(0)

python = str(VENV_PY) if VENV_PY.exists() else sys.executable

failures = []
for s in scripts:
    print(f"\n== {s.name} ==", flush=True)
    r = subprocess.run([python, str(s)])
    if r.returncode != 0:
        failures.append(s.name)

print()
if failures:
    print(f"FAILED: {', '.join(failures)}")
    sys.exit(1)
print(f"OK — {len(scripts)} part(s) built")
