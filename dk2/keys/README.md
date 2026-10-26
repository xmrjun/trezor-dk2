# Own signing keys (branch dk2-ownkeys)

The stock D002 build verifies everything with Trezor's **public development keys**:
anyone can sign a firmware such a device accepts. This branch keeps the devel build
variant (no PRODUCTION: no RDP lock, no automatic bootloader replacement) but swaps
every development key set for our own.

| Key set (`keys.json`) | Checked by | Checks | Source in the build |
|---|---|---|---|
| `root_slh_dsa` + `root_ed25519` (2 + 2, both sign) | boardloader | bootloader boot header (SLH-DSA-SHA2-128s + Ed25519) | `sec/image/inc/sec/root_keys.h` → `xw_keys.h` |
| `bootloader` (2 of 3) | bootloader | vendor header | `sec/image/image.c` → `xw_keys.h` |
| `secmon` (2 of 3) | bootloader | secmon image | `sec/image/image.c` → `xw_keys.h` |
| `vendor` (2 of 3) | bootloader | firmware header | our vendor header `vendor_xw.json` |
| `translations` (2 of 3, all 3 sign) | firmware | language blobs (dk2/zh) | `rust/src/translations/public_keys.rs` → `xw_public_keys.rs` |

Not replaced, not used on D002: the legacy `BOARDLOADER_KEYS` (TRZB bootloader header;
D002 uses the boot UCB / TRZQ header) and `MODEL_*_KEYS` in `models/D002/model_D002.h`
(only used by non-devel builds, which this branch doesn't make).

## Use

```
.venv/bin/python dk2/keys/gen_keys.py ~/esp/hw/wallet/<set>/keys.json <set>   # once
XW_KEYS=~/esp/hw/wallet/<set>/keys.json dk2/build.sh
XW_KEYS=~/esp/hw/wallet/<set>/keys.json .venv/bin/python dk2/zh/zh_tool.py blob
```

`dk2/build.sh` runs `apply_keys.py` first (public halves into the build, our vendor
header signed) and points the build at our vendor header. All signing goes through
trezorlib's "dev keys" paths, which `python/src/trezorlib/_internal/firmware_headers.py`
redirects to `$XW_KEYS`; without `$XW_KEYS` they refuse to sign.

The key file holds private keys (mode 0600). Never commit it, keep a copy offline.
Losing it means the device can't be updated any more except by a full erase.

## Locking to RDP Level 1 (tested on the DK2, 2026-10-26)

The boardloader never touches the RDP level (`sec/option_bytes/stm32u5/option_bytes.c`,
PRODUCTION builds are refused), the SWD-access tamper (ITAMP6) is off, so locking and
unlocking are done by hand. Order matters:

1. RDP 0, TrustZone on (`-ob TZEN=1`), flash **boardloader + bootloader only**
   (`-e all -w boardloader.bin 0x0C004000 -w bootloader.bin 0x0C01E000 -hardRst`);
   the boardloader sets the other option bytes on that first boot.
2. Lock: `STM32_Programmer_CLI -c port=SWD mode=UR -ob RDP=0xBB`. The two
   "Uploading Option Bytes ... failed" errors at the end are expected (read-back is
   blocked once locked).
3. **Unplug CN8 and CN2, wait 10 s, plug CN2 only.** After a debugger session the chip
   blocks flash access until a power-on reset (black screen); booting the wallet in that
   state corrupted its storage once.
4. Install the firmware over USB in the bootloader (`trezorctl firmware update -s -f
   firmware.bin`), then the language blob, then set up / recover the wallet.

**Storage has to be created at Level 1.** Its KEK goes through SAES with the hardware
key (DHUK/XORK), which differs between RDP 0 and RDP 1: a wallet storage created
before locking can't be opened afterwards (RuntimeError at boot). Hence no firmware
boot before step 3.

Locked: debugger can attach (hotplug) and read OPTR, but not flash; firmware updates
signed with our keys work over USB; dev-key-signed images are refused.

Ways back to RDP 0 (both erase the whole flash; both tested):
- bootloader menu (≡) → **Factory reset** → restart: the bootloader wipes, then regresses
  RDP itself (`option_bytes_rdp_regression()`; only this on-device path, not the
  WipeDevice host message). Power-cycle before attaching the debugger.
- debugger: `STM32_Programmer_CLI -c port=SWD mode=HOTPLUG -ob RDP=0xAA TZEN=0`.
Afterwards TZEN is 0 (`-ob TZEN=1` again before flashing); SECBOOTADD0/BOOT_LOCK survive.
