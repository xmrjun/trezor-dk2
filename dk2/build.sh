#!/bin/bash
# Builds Trezor firmware for the STM32U5G9J-DK2 (model D002, board "dk2": RGB LCD + GT911 touch,
# our port; the stock "dk" board is the DSI kit), devel build variant, but signed with our
# own key set instead of the public development keys (dk2/keys/README.md):
#   XW_KEYS=<keys.json> dk2/build.sh            -> boardloader, bootloader, firmware
#   XW_KEYS=<keys.json> dk2/build.sh firmware   -> just one part
# Native build without Nix, tools as in flake.nix (see dk2/README.md).
# Output: core/build/artifacts/D002/{boardloader,bootloader,firmware}.bin
set -e
# ARM GCC 13.3 and the pinned Rust nightly must be on PATH, see dk2/README.md
export LIBCLANG_PATH="${LIBCLANG_PATH:-/Library/Developer/CommandLineTools/usr/lib}"
export SOURCE_DATE_EPOCH=1600000000
cd "$(dirname "$0")/.."
source .venv/bin/activate
: "${XW_KEYS:?set XW_KEYS to our key file (dk2/keys/README.md)}"
export XW_KEYS="$(cd "$(dirname "$XW_KEYS")" && pwd)/$(basename "$XW_KEYS")"
python dk2/keys/apply_keys.py
export VENDOR_HEADER="$PWD/dk2/keys/build/vendorheader_xw_signed.bin"
cd core/embed
parts="${@:-boardloader bootloader firmware}"
for p in $parts; do
  echo "=== building $p (d002, devel, own keys) ==="
  cargo xtask build "$p" -m d002 -b dk2 --bootloader-devel
  if [ "$p" = bootloader ]; then
    # The firmware embeds a bootloader image (installed by PRODUCTION builds only); make it
    # ours instead of the stock one signed with the public development keys.
    cp ../build/artifacts/D002/bootloader.bin models/D002/bootloaders/bootloader_D002_devel.bin
  fi
done
