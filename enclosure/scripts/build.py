"""Build all production SVG files for both clocks.

Usage: python -m enclosure.scripts.build  (from repo root)
       OR: python enclosure/scripts/build.py
"""
from pathlib import Path
import sys

# Allow running as script OR module
if __name__ == "__main__" and __package__ is None:
    sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from enclosure.scripts.render_face import render_face_svg
from enclosure.scripts.render_frame import render_frame_svg

REPO_ROOT = Path(__file__).resolve().parents[2]
ENCLOSURE_DIR = REPO_ROOT / "enclosure"


def main() -> None:
    ENCLOSURE_DIR.mkdir(parents=True, exist_ok=True)

    # Frame geometry is kid-agnostic — one file, used for both clocks
    # (different wood material selected per clock when uploading to Ponoko).
    outputs = {
        "emory-face.svg": lambda: render_face_svg(kid="emory"),
        "nora-face.svg": lambda: render_face_svg(kid="nora"),
        "frame.svg": lambda: render_frame_svg(),
    }

    for filename, generator in outputs.items():
        target = ENCLOSURE_DIR / filename
        target.write_text(generator())
        print(f"  wrote {target.relative_to(REPO_ROOT)} ({target.stat().st_size} bytes)")


if __name__ == "__main__":
    main()
