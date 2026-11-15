"""Saves the panel password for ota_push.py in firmware/.panel_password (ignored by git).
Run it in your own terminal; the typed password is not shown and never goes through any chat.
Usage: python firmware/tools/save_password.py
"""
import getpass
import os

path = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), ".panel_password")
pw = getpass.getpass("Panel password (hidden): ")
if not pw:
    raise SystemExit("empty, nothing saved")
with open(path, "w", encoding="utf-8") as f:
    f.write(pw)
print("saved to " + path)
