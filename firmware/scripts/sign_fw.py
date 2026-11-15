# PlatformIO post-script (cycle 021): writes firmware.signed.bin = image + HMAC-SHA256 + "STSIG001".
# The key is OTA_SIGN_KEY from src/secrets.h (untracked). Without it nothing is written and the panel accepts the plain file.
import hashlib
import hmac
import os
import re

Import("env")  # noqa: F821 (provided by PlatformIO)


def read_key():
    path = os.path.join(env.subst("$PROJECT_DIR"), "src", "secrets.h")  # noqa: F821
    try:
        text = open(path, encoding="utf-8").read()
    except OSError:
        return None
    m = re.search(r'^\s*#define\s+OTA_SIGN_KEY\s+"([^"]+)"', text, re.M)
    return m.group(1) if m else None


def sign(source, target, env):  # noqa: ARG001
    key = read_key()
    src = str(target[0])
    if not key:
        print("sign_fw: OTA_SIGN_KEY not set in src/secrets.h, no signed file written")
        return
    image = open(src, "rb").read()
    mac = hmac.new(key.encode(), image, hashlib.sha256).digest()
    out = os.path.join(os.path.dirname(src), "firmware.signed.bin")
    open(out, "wb").write(image + mac + b"STSIG001")
    print("sign_fw: wrote " + out)


env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", sign)  # noqa: F821
