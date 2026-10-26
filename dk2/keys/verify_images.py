#!/usr/bin/env python3
"""Check a build against a key set using only its PUBLIC keys.

  .venv/bin/python dk2/keys/verify_images.py <keys.json>   (run WITHOUT $XW_KEYS)

Every signature must verify with our keys, and must NOT verify with the public
development keys the stock build uses. Exits non-zero on any failure.
"""

import json
import sys
from pathlib import Path

from slhdsa import PublicKey, sha2_128s
from trezorlib import _ed25519, cosi
from trezorlib._internal import firmware_headers as fh
from trezorlib.firmware import models

ROOT = Path(__file__).resolve().parents[2]
ART = ROOT / "core/build/artifacts/D002"
SECMON = ROOT / "core/build/thumbv8m.main-none-eabihf/release/secmon.bin"

# the stock development public keys (models.py / image.c / vendor_dev_DO_NOT_SIGN.json)
DEV = {
    "root_ed25519": [k.hex() for k in models.ROOT_ED25519_KEYS_DEV],
    "root_slh_dsa": [k.hex() for k in models.ROOT_SLH_DSA_KEYS_DEV_PUBLIC],
    "bootloader": ["d759793bbc13a2819a827c76adb6fba8a49aee007f49f2d0992d99b825ad2c48",
                   "6355691c178a8ff91007a7478afb955ef7352c63e7b25703984cf78b26e21a56",
                   "ee93a4f66f8d16b819bb9beb9ffccdfcdc1412e87fee6a324c2a99a1e0e67148"],
    "secmon": ["db995fe25169d141cab9bbba92baa01f9f2e1ece7df4cb2ac05190f37fcc1f9d",
               "2152f8d19b791d24453242e15f2eab6cb7cffa7b6a5ed30097960e069881db12",
               "22fc297792f0b6ffc0bfcfdb7edb0c0aa14e025a365ec0e342e86e3829cb74b6"],
    "vendor": ["e28a8970753332bd72fef413e6b0b2ef1b4aadda7aa2c141f233712a6876b351",
               "d4eec1869fb1b8a4e817516ad5a931557cb56805c3eb16e8f3a803d647df7869",
               "772c8a442b7db06e166cfbc1ccbcbcde6f3eba76a4e98ef3ffc519502237d6ef"],
    # translations and coin/token definitions (rust/src/{translations,definitions})
    "translations": ["68460ebef3b138164ec7fd8610e95800df7598f70f2f2ea7db5172ac74ebc144",
                     "8d4abe074fef9229d3b441dfea4f98f805b1a2b3a06ae645810efece77fd5044",
                     "97f7135a9a2690e73beb26556f1cb163bea2532affa1e7782430be98c0e56812"],
}

failed = False


def check(what: str, ours: bool, dev: bool) -> None:
    global failed
    ok = ours and not dev
    failed |= not ok
    print(f"{'OK  ' if ok else 'FAIL'} {what}: our keys {'valid' if ours else 'INVALID'},"
          f" public dev keys {'VALID' if dev else 'invalid'}")


def cosi_ok(sig: bytes, digest: bytes, m: int, pubs: list[str], mask: int) -> bool:
    try:
        cosi.verify(sig, digest, m, [bytes.fromhex(p) for p in pubs], mask)
        return True
    except Exception:
        return False


def boot_header_ok(bl: fh.BootloaderV2Image, ec: list[str], pq: list[str]) -> bool:
    digest = bl.merkle_root()
    hash_fn = bl.get_hash_params().hash_function
    idx = [i for i in range(8) if bl.header.sigmask & (1 << i)]
    if len(idx) != len(bl.unauth.ec_signatures) or max(idx) >= len(ec):
        return False
    for n, i in enumerate(idx):
        slh = bl.unauth.slh_signatures[n]
        try:
            _ed25519.checkvalid(bl.unauth.ec_signatures[n],
                                hash_fn(digest + slh).digest(), bytes.fromhex(ec[i]))
        except Exception:
            return False
        if not PublicKey.from_digest(bytes.fromhex(pq[i]), sha2_128s).verify(digest, slh):
            return False
    return True


def main() -> None:
    keys = json.loads(Path(sys.argv[1]).read_text())
    our = {role: [k["pub"] for k in keys[role]] for role in DEV}

    bl = fh.BootloaderV2Image.parse((ART / "bootloader.bin").read_bytes())
    check("bootloader boot header",
          boot_header_ok(bl, our["root_ed25519"], our["root_slh_dsa"]),
          boot_header_ok(bl, DEV["root_ed25519"], DEV["root_slh_dsa"]))

    sm_data = SECMON.read_bytes()
    sm = fh.SecmonImage.parse(sm_data)
    h = sm.header
    check("secmon header", cosi_ok(h.signature, sm.digest(), 2, our["secmon"], h.sigmask),
          cosi_ok(h.signature, sm.digest(), 2, DEV["secmon"], h.sigmask))
    sm.validate_code_hash()

    fwdata = (ART / "firmware.bin").read_bytes()
    fw = fh.VendorFirmware.parse(fwdata)
    vh = fw.vendor_header
    check("vendor header", cosi_ok(vh.signature, vh.digest(), 2, our["bootloader"], vh.sigmask),
          cosi_ok(vh.signature, vh.digest(), 2, DEV["bootloader"], vh.sigmask))
    vpubs = [p.hex() for p in vh.pubkeys]
    check("vendor header carries our firmware keys", vpubs == our["vendor"], vpubs == DEV["vendor"])
    fh_ = fw.firmware.header
    check("firmware header", cosi_ok(fh_.signature, fw.digest(), vh.sig_m, our["vendor"], fh_.sigmask),
          cosi_ok(fh_.signature, fw.digest(), vh.sig_m, DEV["vendor"], fh_.sigmask))
    hashes_ok = list(fh_.hashes) == list(fw.firmware.code_hashes())
    check("firmware code hashes match the header", hashes_ok, False)
    check("signed secmon is the one inside firmware.bin", sm_data in fwdata, False)

    # no public development key may be left in any image, ours must be where they belong
    images = {n: (ART / f"{n}.bin").read_bytes() for n in ("boardloader", "bootloader", "firmware")}
    for name, data in images.items():
        left = [f"{role}[{i}]" for role, ks in DEV.items() for i, k in enumerate(ks)
                if bytes.fromhex(k) in data]
        check(f"{name}.bin has no public dev key {left or ''}".rstrip(), not left, bool(left))
    has = lambda name, role: all(bytes.fromhex(k) in images[name] for k in our[role])
    check("boardloader.bin holds our root keys", has("boardloader", "root_ed25519")
          and has("boardloader", "root_slh_dsa"), False)
    check("bootloader.bin holds our bootloader + secmon keys",
          has("bootloader", "bootloader") and has("bootloader", "secmon"), False)
    check("firmware.bin holds our translation keys", has("firmware", "translations"), False)
    check("firmware.bin embeds this very bootloader", images["bootloader"] in images["firmware"], False)

    print("ALL OK" if not failed else "FAILED")
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
