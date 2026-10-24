#!/bin/bash
# Flashes the D002 (STM32U5G9J-DK2) build over the on-board ST-LINK (CN8) with
# STM32CubeProgrammer. TrustZone must be enabled first, see dk2/README.md.
#   dk2/flash.sh            -> erase the whole flash, write all three images
#   dk2/flash.sh firmware   -> erase the firmware area only and write firmware
#                              (keeps the wallet storage and the language pack)
set -e
CLI="${STM32_PROGRAMMER_CLI:-/Applications/STMicroelectronics/STM32Cube/STM32CubeProgrammer/STM32CubeProgrammer.app/Contents/Resources/bin/STM32_Programmer_CLI}"
A="$(dirname "$0")/../core/build/artifacts/D002"

if [ "$1" = "firmware" ]; then
  # The bootloader rejects the firmware ("FIRMWARE CORRUPTED") if leftovers of
  # a previous, longer image remain in the firmware area, erase all of it.
  "$CLI" -c port=SWD mode=UR -e '[47 463]' -w "$A/firmware.bin" 0x0C05E000 -v -hardRst
else
  "$CLI" -c port=SWD mode=UR -e all \
    -w "$A/boardloader.bin" 0x0C004000 -v \
    -w "$A/bootloader.bin" 0x0C01E000 -v \
    -w "$A/firmware.bin" 0x0C05E000 -v -hardRst
fi
