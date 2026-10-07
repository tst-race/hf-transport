#!/usr/bin/env python3
"""
Setup script for HF Stego Transport integration test.
Copies built plugin artifacts to test/kits directory.
"""

import shutil
import platform
from pathlib import Path

script_dir = Path(__file__).parent.resolve()
plugin_root = script_dir.parent

# Detect architecture
machine = platform.machine().lower()
if machine in {"x86_64", "amd64"}:
    arch = "x86_64"
elif machine in {"arm64", "aarch64"}:
    arch = "arm64-v8a"
else:
    raise ValueError(f"Unsupported architecture: {platform.machine()}")

KIT_NAME = "PluginHFTransport"
source = plugin_root / "kit" / "artifacts" / f"linux-{arch}-server" / KIT_NAME
dest = script_dir / "kits" / KIT_NAME

print(f"Copying kit from {source} to {dest}")

if dest.exists():
    shutil.rmtree(dest)

dest.parent.mkdir(parents=True, exist_ok=True)

if source.exists():
    shutil.copytree(source, dest)
    print("Kit copied successfully")
else:
    print(f"Warning: Source kit not found at {source}")
    print("Run build_artifacts_in_docker_image.sh first")
    import sys
    sys.exit(1)