#!/usr/bin/env python3
"""Reject a release whose tag disagrees with its Windows/CMake metadata."""
import argparse
import re
import sys
import xml.etree.ElementTree as ET
from pathlib import Path


def check_version(root, tag):
    match = re.fullmatch(r"v(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)", tag)
    if not match:
        raise ValueError("Release tag must be vMAJOR.MINOR.PATCH, for example v0.0.1")
    parts = match.groups()
    if any(int(part) > 65535 for part in parts):
        raise ValueError("Windows file version components must not exceed 65535")
    version = ".".join(parts)
    windows_version = ",".join((*parts, "0"))
    cmake = (root / "CMakeLists.txt").read_text(encoding="utf-8")
    declared = re.search(r"project\(LittleTimer\s+VERSION\s+(\S+)", cmake)
    if not declared or declared.group(1) != version:
        raise ValueError(f"CMakeLists.txt must declare version {version}")
    resources = (root / "resources/app.rc").read_text(encoding="utf-8")
    for field in ("FILEVERSION", "PRODUCTVERSION"):
        declared = re.search(rf"^{field}\s+([^\r\n]+)$", resources, re.MULTILINE)
        if not declared or re.sub(r"\s+", "", declared.group(1)) != windows_version:
            raise ValueError(f"resources/app.rc {field} must be {windows_version}")
    for field in ("FileVersion", "ProductVersion"):
        declared = re.search(rf'VALUE\s+"{field}"\s*,\s*"([^"\\]+)\\0"', resources)
        if not declared or declared.group(1) != version:
            raise ValueError(f"resources/app.rc {field} must be {version}")
    manifest = ET.parse(root / "resources/app.manifest")
    identity = manifest.getroot().find("{urn:schemas-microsoft-com:asm.v1}assemblyIdentity")
    if identity is None or identity.get("version") != version + ".0":
        raise ValueError(f"resources/app.manifest must declare version {version}.0")
    notes = root / "docs/releases" / f"{tag}.md"
    if not notes.is_file() or not notes.read_text(encoding="utf-8").strip():
        raise ValueError(f"Missing release notes: docs/releases/{tag}.md")
    return version


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("tag")
    args = parser.parse_args()
    try:
        version = check_version(Path(__file__).resolve().parents[1], args.tag)
    except (ValueError, OSError, ET.ParseError) as error:
        print(f"Release version check failed: {error}", file=sys.stderr)
        sys.exit(1)
    print(f"PASS: {args.tag}, CMake {version}, Windows {version}.0, release notes present")
