#!/usr/bin/env python3
"""Generate our own signing key set (replaces the public development keys).

  .venv/bin/python dk2/keys/gen_keys.py <out.json> [name]

The file holds PRIVATE keys: it is created with mode 0600 and never overwritten.
Keep it out of the repository and back it up; whoever has it can sign firmware
the device will accept, and without it the device can't be updated any more
(only an RDP regression with a full erase gets it back).

Roles (what verifies them on the device):
  root_slh_dsa, root_ed25519  boardloader checks the bootloader boot header
                              (both keys sign, SLH-DSA-SHA2-128s + Ed25519)
  bootloader                  bootloader checks the vendor header (2 of 3)
  secmon                      bootloader checks the secmon image (2 of 3)
  vendor                      bootloader checks the firmware (2 of 3, keys
                              listed in our vendor header)
  translations                firmware checks language blobs (2 of 3)
"""

import json
import os
import secrets
import sys

import slhdsa
from trezorlib import cosi

ED25519_ROLES = {"root_ed25519": 2, "bootloader": 3, "secmon": 3, "vendor": 3, "translations": 3}
SLH_DSA_ROLES = {"root_slh_dsa": 2}


def ed25519_key() -> dict:
    priv = secrets.token_bytes(32)
    return {"priv": priv.hex(), "pub": cosi.pubkey_from_privkey(priv).hex()}


def slh_dsa_key() -> dict:
    kp = slhdsa.KeyPair.gen(slhdsa.sha2_128s)
    return {"priv": kp.sec.digest().hex(), "pub": kp.pub.digest().hex()}


def main() -> None:
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    out = sys.argv[1]
    name = sys.argv[2] if len(sys.argv) > 2 else os.path.splitext(os.path.basename(out))[0]
    keys = {"name": name}
    for role, n in SLH_DSA_ROLES.items():
        keys[role] = [slh_dsa_key() for _ in range(n)]
    for role, n in ED25519_ROLES.items():
        keys[role] = [ed25519_key() for _ in range(n)]
    # O_EXCL: never overwrite an existing key file
    fd = os.open(out, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    with os.fdopen(fd, "w") as f:
        json.dump(keys, f, indent=1)
    print(f"wrote {out} ({name}); public keys:")
    for role in [*SLH_DSA_ROLES, *ED25519_ROLES]:
        for i, k in enumerate(keys[role]):
            print(f"  {role}[{i}] {k['pub']}")


if __name__ == "__main__":
    main()
