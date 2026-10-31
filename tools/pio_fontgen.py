# PlatformIO pre-build hook: regenerate the generated headers when they are
# missing or older than the script that makes them. Runs the system Python
# (which has Pillow), not PlatformIO's own environment.
#   tools/fontgen.py -> src/generated/fonts_gen.h
#   tools/logogen.py -> src/generated/logos_gen.h (downloads logos it lacks)
import os
import shutil
import subprocess

Import("env")  # noqa: F821  (provided by PlatformIO)

root = env["PROJECT_DIR"]  # noqa: F821
py = shutil.which("python") or shutil.which("python3") or "python"

for script, header in (("fontgen.py", "fonts_gen.h"), ("logogen.py", "logos_gen.h")):
    gen = os.path.join(root, "tools", script)
    out = os.path.join(root, "src", "generated", header)
    if not os.path.exists(out) or os.path.getmtime(out) < os.path.getmtime(gen):
        subprocess.check_call([py, gen])
