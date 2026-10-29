# PlatformIO pre-script: defines FW_VERSION from `git describe` (R7, cycle 001).
import subprocess

Import("env")  # noqa: F821 (provided by PlatformIO)


def firmware_version():
    try:
        out = subprocess.check_output(
            ["git", "describe", "--tags", "--always", "--dirty"],
            cwd=env.subst("$PROJECT_DIR"),  # noqa: F821
            stderr=subprocess.DEVNULL,
        )
        return out.decode().strip() or "0.0.0-dev"
    except Exception:
        return "0.0.0-dev"


env.Append(CPPDEFINES=[("FW_VERSION", '\\"%s\\"' % firmware_version())])  # noqa: F821
