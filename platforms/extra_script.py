Import("env")
import os

try:
    Import("pio_lib_builder")
except Exception:
    pio_lib_builder = None

try:
    Import("projenv")
except Exception:
    projenv = None


def _read_lobbs_version(version_path):
    major, minor, build = 2, 0, 0
    if not os.path.isfile(version_path):
        return None, None
    with open(version_path) as f:
        in_section = False
        for line in f:
            line = line.strip()
            if line == "[LOBBS]":
                in_section = True
                continue
            if line.startswith("[") and line.endswith("]"):
                in_section = False
                continue
            if not in_section:
                continue
            if line.startswith("major="):
                major = int(line.split("=", 1)[1])
            elif line.startswith("minor="):
                minor = int(line.split("=", 1)[1])
            elif line.startswith("build="):
                build = int(line.split("=", 1)[1])
    long_ver = f"{major}.{minor}.{build}"
    short_ver = f"{major}.{minor}"
    return long_ver, short_ver


def _lobbs_version_path():
    if pio_lib_builder is not None:
        return os.path.join(pio_lib_builder.path, "version.properties")
    project_dir = env.subst("$PROJECT_DIR")
    for root in (
        project_dir,
        os.path.normpath(os.path.join(project_dir, "..", "..")),
    ):
        candidate = os.path.join(root, "version.properties")
        if os.path.isfile(candidate):
            return candidate
    return os.path.join(project_dir, "version.properties")


def _inject_version(e):
    if e is None:
        return
    version_path = _lobbs_version_path()
    long_ver, short_ver = _read_lobbs_version(version_path)
    if not long_ver:
        return
    e.Append(
        CPPDEFINES=[
            ("LOBBS_VERSION", '\\"' + long_ver + '\\"'),
            ("LOBBS_VERSION_SHORT", '\\"' + short_ver + '\\"'),
        ]
    )


_inject_version(env)
_inject_version(projenv)
