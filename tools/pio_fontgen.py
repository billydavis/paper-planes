# PlatformIO pre-build hook: regenerate src/generated/fonts_gen.h when it is
# missing or older than tools/fontgen.py. Runs the system Python (which has
# Pillow), not PlatformIO's own environment.
import os
import shutil
import subprocess

Import("env")  # noqa: F821  (provided by PlatformIO)

root = env["PROJECT_DIR"]  # noqa: F821
gen = os.path.join(root, "tools", "fontgen.py")
out = os.path.join(root, "src", "generated", "fonts_gen.h")

if not os.path.exists(out) or os.path.getmtime(out) < os.path.getmtime(gen):
    py = shutil.which("python") or shutil.which("python3") or "python"
    subprocess.check_call([py, gen])
