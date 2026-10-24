#!/usr/bin/env python3
"""Builds the Chinese (zh-CN) translation for the D002 port with the eckhart
layout.

  zh_tool.py json   - merge zh_strings_*.json into zh.json (untranslated keys
                      stay English)
  zh_tool.py blob   - build translation-D002-zh-CN-<version>.bin, signed with
                      the development keys (accepted by --bootloader-devel builds)

The CJK glyphs are not part of the blob (they would not fit), they are built
into the firmware, see gen_cjk_font.py and rust/src/ui/display/cjk.rs.
"""

import dataclasses
import json
import sys
from pathlib import Path

from trezorlib import cosi, merkle_tree, models
from trezorlib._internal import translations

HERE = Path(__file__).parent
TRANSLATIONS = HERE.parents[1] / "core/translations"
ZH_JSON = HERE / "zh.json"
LAYOUT = "Eckhart"
PRIVATE_KEYS_DEV = [byte * 32 for byte in (b"\xdd", b"\xde", b"\xdf")]


def load_zh_strings() -> dict[str, str]:
    strings: dict[str, str] = {}
    for path in sorted(HERE.glob("zh_strings_*.json")):
        for key, value in json.loads(path.read_text(encoding="utf-8")).items():
            if key in strings:
                raise ValueError(f"{key} translated twice ({path.name})")
            strings[key] = value
    return strings


def check_placeholders(key: str, en: str, zh: str) -> None:
    import re

    pattern = r"\{[^}]*\}"
    if sorted(re.findall(pattern, en)) != sorted(re.findall(pattern, zh)):
        raise ValueError(f"{key}: placeholders differ: {en!r} -> {zh!r}")
    if ("|" in en) != ("|" in zh):
        raise ValueError(f"{key}: plural form differs: {en!r} -> {zh!r}")


def make_json() -> None:
    en = json.loads((TRANSLATIONS / "en.json").read_text(encoding="utf-8"))
    zh_strings = load_zh_strings()

    unknown = set(zh_strings) - set(en["translations"])
    if unknown:
        raise ValueError(f"unknown keys: {sorted(unknown)}")

    result = {}
    translated = 0
    for key, value in en["translations"].items():
        if key not in zh_strings:
            result[key] = value
            continue
        zh = zh_strings[key]
        if isinstance(value, dict):
            check_placeholders(key, value[LAYOUT], zh)
            result[key] = {**value, LAYOUT: zh}
        else:
            check_placeholders(key, value, zh)
            result[key] = zh
        translated += 1

    data = {
        "fonts": {LAYOUT: {}},
        "header": {"language": "zh-CN", "version": en["header"]["version"]},
        "translations": result,
    }
    ZH_JSON.write_text(
        json.dumps(data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
    )
    print(f"{ZH_JSON}: {translated}/{len(result)} translated")


def make_blob() -> None:
    lang_data = json.loads(ZH_JSON.read_text(encoding="utf-8"))
    order = translations.order_from_json(
        json.loads((TRANSLATIONS / "order.json").read_text())
    )
    model = dataclasses.replace(models.D002, layout=LAYOUT)
    version = translations.version_from_json(lang_data["header"]["version"])

    blob = translations.blob_from_defs(
        lang_data, order, model, version, TRANSLATIONS / "fonts"
    )
    tree = merkle_tree.MerkleTree([blob.header_bytes])
    signature = cosi.sign_with_privkeys(
        tree.get_root_hash(), PRIVATE_KEYS_DEV, deterministic=True
    )
    blob.proof = translations.Proof(
        merkle_proof=tree.get_proof(blob.header_bytes),
        signature=signature,
        sigmask=0b111,
    )
    v = blob.header.version
    out = HERE / f"translation-D002-zh-CN-{v[0]}.{v[1]}.{v[2]}.bin"
    data = blob.build()
    out.write_bytes(data)
    print(f"{out}: {len(data)} bytes")


if __name__ == "__main__":
    {"json": make_json, "blob": make_blob}[sys.argv[1]]()
