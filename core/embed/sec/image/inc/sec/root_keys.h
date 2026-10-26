/*
 * This file is part of the Trezor project, https://trezor.io/
 *
 * Copyright (c) SatoshiLabs
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

/*
 * Root public keys for pq_secure_boot -- ONE key set, ALL models.
 *
 * Named after their trezorlib counterparts (firmware/models.py), which is the
 * authority and what the signer uses, so the two sides can be compared by name:
 *
 *     ROOT_SLH_DSA_KEYS      <- ROOT_SLH_DSA_KEYS            (production)
 *     ROOT_ED25519_KEYS      <- ROOT_ED25519_KEYS            (production)
 *     ROOT_SLH_DSA_KEYS_DEV  <- ROOT_SLH_DSA_KEYS_DEV_PUBLIC (devel)
 *     ROOT_ED25519_KEYS_DEV  <- ROOT_ED25519_KEYS_DEV        (devel)
 *
 * The _DEV names drop Python's _PUBLIC: C only ever holds public halves, so
 * there is no private counterpart here to distinguish them from.
 */

// clang-format off

/*
 * DEVEL/QA pool. These PUBLIC keys must NEVER be built into a shipped device.
 *
 * Their private halves are public knowledge -- the SLH-DSA ones are checked into
 * this repository (trezorlib firmware/models.py ROOT_SLH_DSA_KEYS_DEV_PRIVATE)
 * and the Ed25519 ones are not even stored, but derived from the constants 'A'
 * and 'B' (_internal/firmware_headers.py _make_dev_keys). Anyone can therefore
 * sign a boot header that a bootloader carrying these keys will accept, so such a
 * build has no secure boot at all. It is a development convenience, selected by
 * BOOTLOADER_DEVEL, and nothing more.
 */
/* dk2: the devel pool is replaced by our own keys, generated into xw_keys.h
 * by dk2/keys/apply_keys.py (the build fails if it is missing). */
#include <sec/xw_keys.h>
#define ROOT_SLH_DSA_KEYS_DEV XW_ROOT_SLH_DSA_KEYS
#define ROOT_ED25519_KEYS_DEV XW_ROOT_ED25519_KEYS

/*** PRODUCTION ***/
#define ROOT_SLH_DSA_KEYS \
  (const uint8_t *)"\xec\x57\xa2\x64\x3e\x55\x3c\x59\x19\x47\x3c\xd5\x79\xcd\xdd\xa6\x50\x05\x7c\x2f\xd5\x98\xa4\x47\x57\x4b\xdb\x6c\x1f\x0f\x55\x21", \
  (const uint8_t *)"\xd2\x96\xd8\xcf\x9b\xe3\xe9\x23\xe1\x0a\xc0\x3f\x43\x56\x6d\x18\x9d\x11\xf6\xb5\xdd\xab\xdf\x8d\xc1\x2d\x29\xc0\x0e\x5a\x13\x6a", \
  (const uint8_t *)"\xb7\x2b\xd7\x1b\xf8\xe1\x09\xd3\x77\x4d\x91\xe3\xc1\xab\xd2\xa2\xe9\xff\x6b\x57\x11\x89\x6f\x8d\x87\x3a\x3d\xf9\xb9\xbe\x98\xd1",

#define ROOT_ED25519_KEYS \
  (const uint8_t *)"\xb0\xd7\x3e\x86\xae\x39\x2a\x26\xda\x72\x75\x99\x4e\x96\x50\x97\xae\x7e\xe8\xf8\x84\x55\x78\x8e\x8c\x53\x40\x21\xd5\xde\x18\x85", \
  (const uint8_t *)"\xa8\xf1\x8b\x94\x86\x16\x7c\x97\xb0\x59\xfd\x4f\x05\x3b\xe8\x24\xf7\xd5\xb0\xcb\x87\x10\xb3\xca\x12\xd2\x6d\x2d\xda\xc3\x51\xc9", \
  (const uint8_t *)"\xd2\x0f\xbd\xa4\x27\x1a\xeb\x06\xc1\x8c\x26\xc6\xf2\xad\xb6\xd6\xb0\xe3\x12\xf8\x45\xf9\x04\x41\xfd\x61\x4f\x39\x75\x54\xa8\x6e",

// clang-format on
