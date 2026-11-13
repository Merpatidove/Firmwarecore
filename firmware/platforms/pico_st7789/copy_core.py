# copy_core.py — pull rsvp_core.c/h into src/ before build so the platform
# build compiles the core without touching the shared source tree.
import shutil
from pathlib import Path

Import("env")

here = Path(env["PROJECT_DIR"])
core = here.parent.parent  # firmware/
src = here / "src"
for name in ("rsvp_core.c", "rsvp_core.h"):
    shutil.copy2(core / name, src / name)
print("copy_core: rsvp_core.c/h -> src/")
