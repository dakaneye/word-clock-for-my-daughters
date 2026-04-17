# firmware/scripts/pio_generate_captive_html.py
# Pre-build hook that runs gen_form_html.py for the current env.
from pathlib import Path
import subprocess
import sys

Import("env")  # type: ignore  # provided by PlatformIO at build time

ENV_NAME = env["PIOENV"]
KID = ENV_NAME  # our envs are named "emory" and "nora"

if KID not in ("emory", "nora"):
    # Skip for native / any unknown env — they don't need the header.
    Return()  # type: ignore

ROOT = Path(env["PROJECT_DIR"])
GEN = ROOT / "assets" / "captive-portal" / "gen_form_html.py"
OUT = ROOT / "lib" / "wifi_provision" / "src" / "form_html.h"

print(f"[captive-portal] generating {OUT.relative_to(ROOT)} for kid={KID}")
subprocess.run(
    [sys.executable, str(GEN), "--kid", KID, "--out", str(OUT)],
    check=True,
)
