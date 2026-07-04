# CLAUDE.md — xiaozhi-esp32 (Nexus fork)

XiaoZhi ESP32-S3 firmware. One physical device (**Waveshare ESP32-S3-Touch-LCD-1.85C**)
runs both the **voice endpoint** and **pager-mode** off this same tree.

Full hardware/design SOT lives in the vault: `~/vault/openclaw/specs/esp32/`
(`README.md` = build & flash guideline; `agent-pager/`, `voice-endpoint/`, `ai-usage/`).

## Git workflow (this repo only — overrides the global branch/PR rule)

- **Work directly on `main`. No feature branches, no PRs.** Commit each change straight to `main`.
- **Always `git push` after committing.** Never leave work local.
- **Push target is `fork`** (`git@github.com:abacha/xiaozhi-esp32.git`) — our fork.
  `origin` (`github.com/78/xiaozhi-esp32`) is the read-only upstream we forked from; never push there.
  `main` tracks `fork/main`.

## Toolchain

- **ESP-IDF v5.5.4 at `~/esp-idf-5.5`** (firmware needs ≥5.5.2). **Do NOT use `~/esp-idf` (v5.4)** —
  too old; the upstream README's "5.4" is stale.
- The IDF 5.5 venv is **Python 3.12**, but `export.sh` grabs the system python (3.11) and dies with
  `idf5.5_py3.11_env ... not found`. Put the asdf python (3.12) ahead of it **before** sourcing:
  ```sh
  export PATH="$HOME/.asdf/shims:$PATH"   # or: $HOME/.asdf/installs/python/3.12.2/bin
  source ~/esp-idf-5.5/export.sh
  ```

## Board config (read before touching sdkconfig)

- **Board is selected by a Kconfig symbol, not a cmake flag:**
  `CONFIG_BOARD_TYPE_WAVESHARE_ESP32_S3_TOUCH_LCD_1_85C=y` in `sdkconfig.defaults`.
  `-DBOARD_TYPE=` is **ignored** and falls back to the wrong device (bread-compact-wifi).
- **`sdkconfig.defaults.esp32s3` applies AFTER `sdkconfig.defaults` and overrides it** — put
  target-specific overrides there.
- If config looks stale, delete the generated `sdkconfig` (NOT the `*.defaults`) to force a clean regen.
- 1.85C board files: `main/boards/waveshare/esp32-s3-touch-lcd-1.85c/`. Display + touch (CST816)
  share the same `DISPLAY_MIRROR_X/Y` / `DISPLAY_SWAP_XY` defines in `config.h`, so a 180° flip
  is `MIRROR_X=MIRROR_Y=true` in one place — no LVGL rotation needed.

## Build

```sh
cd ~/xiaozhi-esp32
idf.py set-target esp32s3   # only on a fresh build/ dir
idf.py build                # -> build/xiaozhi.bin
```

## Flash — the board is USB-tethered to the Windows PC, NOT this host

The board's USB serial is **`COM7` on `abacha-pc-wsl`** and is not visible over SSH/WSL. Flash Windows-side:

```sh
scp build/xiaozhi.bin abacha-pc-wsl:/mnt/c/Users/abach/Downloads/
ssh abacha-pc-wsl -- /mnt/c/Windows/System32/WindowsPowerShell/v1.0/powershell.exe -c \
  "python -m esptool --chip esp32s3 --port COM7 --baud 115200 write-flash <offset> C:\Users\abach\Downloads\xiaozhi.bin"
```

- Escape `$` in remote powershell as `\$` (the remote login shell expands it otherwise).
- **Offsets:** app-only **`0x20000`** preserves WiFi NVS — use for iteration. Full image **`0x0`**
  wipes WiFi NVS (redo WiFi via the `xiaozhi` AP). ESP32-S3 bootloader offset is `0x0`, not `0x1000`.
- Close any web-flasher (esptool-js) first — WebSerial holds `COM7` exclusively.

## Verify after flash

- Hash-verify at write time proves the bytes landed, but confirm the running build via the
  **`Compile time:`** line in the boot log (read serial @115200 in powershell; reset by toggling RTS).
- COM7 (native USB-CDC) may not re-enumerate right after a reset — for UI changes the real check is visual.
