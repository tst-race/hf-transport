#!/usr/bin/env python3
"""
adapter.py
HF Stego Transport's plugin-owned test adapter: encapsulates HF-specific config
generation (HF credentials, repo addresses) so the central scenario orchestrator
never needs to know HF's parameter names or cross-node address sharing.
"""

import json
import platform
import sys
from pathlib import Path

sys.path.insert(
    0, str(Path(__file__).resolve().parents[2] / "raceboat" / "test" / "integration")
)
from adapter_types import NodeContribution, NodeRequest  # noqa: E402

CHANNEL_NAME = "hf-stego-transport"

# Fixed repo suffix for integration tests - matches transport default
REPO_SUFFIX = "test"

# Supported slots
SUPPORTED_SLOTS = {"channel", "initial", "final"}

def _detect_host_architecture() -> str:
    machine = platform.machine().lower()
    if machine in {"x86_64", "amd64"}:
        return "x86_64"
    if machine in {"arm64", "aarch64"}:
        return "arm64-v8a"
    raise ValueError(
        f"Unsupported host architecture for Docker builds: {platform.machine()}"
    )


def _kit_dir() -> Path:
    plugin_root = Path(__file__).resolve().parents[1]
    return (
        plugin_root
        / "kit"
        / "artifacts"
        / f"linux-{_detect_host_architecture()}-server"
        / "PluginHFTransport"
    )


def generate_node_contribution(request: NodeRequest) -> NodeContribution:
    # Validate slot
    slot = request.composition_name or request.slot
    if slot not in SUPPORTED_SLOTS:
        raise ValueError(
            f"hf-stego-transport adapter: unsupported slot '{slot}'. "
            f"Supported: {SUPPORTED_SLOTS}"
        )

    # HF Stego Transport is a plain plugin, so --param entries are prefixed
    # with its plugin id (manifest.json plugins[].file_path)
    prefix = "PluginHFTransport"
    
    # HF repo address in JSON format (matching LinkAddress struct)
    # Environment variables will be substituted by docker-compose
    hf_address_json = json.dumps({
        "username": "${HF_USERNAME}",
        "repo_name": f"transport-{REPO_SUFFIX}",
        "commit_sha": "",
        "carrier_model": "${CARRIER_MODEL:-bert-base-uncased}"
    })
    
    contribution = NodeContribution(
        params={
            # HF credentials - provided via docker-compose environment
            f"{prefix}.hf_username": "${HF_USERNAME}",
            f"{prefix}.hf_token": "${HF_TOKEN}",
            f"{prefix}.carrier_model": "${CARRIER_MODEL:-bert-base-uncased}",
            "hostname": request.ip,
        },
        channel_name=CHANNEL_NAME,
        kit_dir=_kit_dir(),
    )

    if request.role == "listener":
        # Listener receives data - provides its HF repo address as recv-address
        # The transport's loadLinkAddress will parse this JSON address
        contribution.cli_flags["recv-address"] = hf_address_json
        # Also publish the HF repo address for connectors (same JSON format)
        contribution.address_output = hf_address_json
    elif request.role == "connector":
        contribution.needs_peer_address = True
        # Connector sends data to listener's repo - uses send-address
        # The transport's createLinkFromAddress will parse this JSON address
        contribution.cli_flags["send-address"] = hf_address_json
        # For LD_BIDI transport, connector does NOT need recv-address
        # The same link is used for both send and receive
    else:
        raise ValueError(f"hf-stego-transport adapter: unknown role '{request.role}'")

    return contribution
