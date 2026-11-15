# Updating the firmware over Wi-Fi (OTA)

## What protects it

1. **Login**: the upload needs a panel session (cookie), like every other change.
2. **Signature** (cycle 021): when `src/secrets.h` has `OTA_SIGN_KEY`, the device installs only a file that ends with `HMAC-SHA256(key, image)` + `STSIG001`. Without the key nobody can make a file the device accepts, even with the panel password. The trailer is held back while receiving, so the flash only gets the image.
3. **Image check**: the ESP-IDF checks header, size and its own SHA-256; the text `SUNTRACKER-FW` must be inside (this only stops a wrong-product file, it is not security).
4. **Probation**: the new image must run 60 s healthy; three failed boots roll back to the old slot by themselves.

Not covered: HTTPS (the panel is plain HTTP on the local network), and protection against installing an *older* signed file.

## How to update

The build writes two files next to each other in `firmware/.pio/build/esp32dev/`: `firmware.bin` and `firmware.signed.bin` (only when the key exists).

```
python firmware/tools/ota_push.py <device-ip>
```

It logs in (password from `secrets.h`, `SUNTRACKER_PASSWORD` or `--password`), uploads `firmware.signed.bin`, waits for the restart and prints the version now running. Or use the panel: ابزار → به‌روزرسانی, and pick the signed file.

**First time on a device that has no key yet** (running an older image): send the plain `firmware.bin` once. That image contains the key; after it every update must be the signed file. Changing `OTA_SIGN_KEY` later needs the same two steps (the old key signs a file that carries the new key).
**Key**: made once with `python -c "import secrets; print(secrets.token_hex(32))"`, stored only in the untracked `secrets.h`. Lose it and the only way back is a USB flash.
