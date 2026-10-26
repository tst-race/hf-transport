#!/usr/bin/env python3
"""
adapter.py
HF Transport test adapter: encapsulates HF Hub link addressing (hf:// URLs)
so the central scenario orchestrator never needs to know this plugin's
address-format requirements.
"""

import json
import platform
import sys
import uuid
from pathlib import Path

try:
    from adapter_types import NodeContribution, NodeRequest
except ImportError:
    _integration_dir = Path(__file__).resolve().parents[2] / "raceboat" / "test" / "integration"
    if not _integration_dir.is_dir():
        raise ImportError("HF Transport adapter: could not import adapter_types")
    sys.path.insert(0, str(_integration_dir))
    from adapter_types import NodeContribution, NodeRequest

DEFAULT_COMPOSITION = "hfTransportComposition"
KIT_NAME = "PluginHFTransport"

# NO sidecar services — HF Hub is external
SIDECAR_SERVICES = {}

def _detect_host_architecture() -> str:
    machine = platform.machine().lower()
    if machine in {"x86_64", "amd64"}:
        return "x86_64"
    if machine in {"arm64", "aarch64"}:
        return "arm64-v8a"
    raise ValueError(f"Unsupported host architecture: {platform.machine()}")

def kit_dir(role: str) -> Path:
    plugin_root = Path(__file__).resolve().parents[1]
    return (
        plugin_root
        / "kit"
        / "artifacts"
        / f"linux-{_detect_host_architecture()}-{'client' if role == 'connector' else 'server'}"
        / KIT_NAME
    )

def _make_address(username: str, repo_id: str) -> str:
    """Create JSON address string for HF repo."""
    return json.dumps({
        "type": "hf",
        "username": username,
        "repoId": repo_id,
        "commitSha": "",
        "branch": "main"
    })

def _deterministic_repo_id(scenario_id: str, role: str, direction: str) -> str:
    """Generate deterministic repo ID based on scenario and role.
    This allows both nodes to predict each other's repo IDs without peer_context.
    """
    # Use scenario_id to namespace, role to distinguish nodes, direction for send/recv
    # With LD_CREATOR_TO_LOADER, each node creates its SEND repo
    # and loads the OTHER node's SEND repo as its recv-address
    safe_scenario = scenario_id.replace('-', '_').replace('.', '_')
    return f"transport-{safe_scenario}-{role}-{direction}"

def generate_node_contribution(request: NodeRequest) -> NodeContribution:
    composition = request.composition_name or DEFAULT_COMPOSITION
    contribution = NodeContribution(
        params={},
        channel_name=composition,
        kit_dir=kit_dir(request.role),
        sidecar_services=dict(SIDECAR_SERVICES),
    )

    # HF Transport uses per-repo unidirectional links with LD_CREATOR_TO_LOADER.
    # Address format: JSON string (required by framework for connection reuse checks)
    # {"type": "hf", "username": "...", "repoId": "...", "commitSha": "...", "branch": "..."}
    #
    # LD_CREATOR_TO_LOADER semantics:
    # - CREATOR (sender) creates the repo and posts data to it
    # - LOADER (receiver) fetches/polls that repo
    # - shouldCreate = true for sender, false for receiver
    #
    # Direction handling:
    # - direction=None (bidi/shared): provide both send and recv addresses
    # - direction="send": this contribution fills the SEND direction (creator creates repo)
    # - direction="recv": this contribution fills the RECV direction (loader loads repo)

    mode = request.scenario_mode if hasattr(request, 'scenario_mode') else 'send'
    username = "${HF_USERNAME}"
    scenario_id = request.scenario_id or "hf-send-recv"
    direction = request.direction  # None, "send", or "recv"

    if request.role == "listener":
        if mode == "send":
            # Send-only: Listener is RECEIVER (loader) - only recv direction
            if direction in (None, "recv"):
                recv_repo_id = _deterministic_repo_id(scenario_id, "connector", "send")
                recv_address = _make_address(username, recv_repo_id)
                contribution.address_output = recv_address
                contribution.cli_flags["recv-address"] = recv_address
                contribution.cli_flags["recv-role"] = "default"

        elif mode == "send-recv":
            # Send-recv: Listener creates its SEND repo, loads connector's SEND repo
            if direction in (None, "send"):
                send_repo_id = _deterministic_repo_id(scenario_id, "listener", "send")
                send_address = _make_address(username, send_repo_id)
                contribution.address_output = send_address
                contribution.cli_flags["send-address"] = send_address
                contribution.cli_flags["send-role"] = "default"
            if direction in (None, "recv"):
                recv_repo_id = _deterministic_repo_id(scenario_id, "connector", "send")
                recv_address = _make_address(username, recv_repo_id)
                contribution.cli_flags["recv-address"] = recv_address
                contribution.cli_flags["recv-role"] = "default"

        elif mode == "client-connect":
            # Client-connect: Listener is server side
            # For split-direction: recv=twoSixIndirect, send=hfTransport
            # If this adapter is called for "send" direction (hfTransport):
            if direction in (None, "send"):
                # Listener creates its SEND repo (downstream: listener->connector)
                send_repo_id = _deterministic_repo_id(scenario_id, "listener", "send")
                send_address = _make_address(username, send_repo_id)
                contribution.address_output = send_address
                contribution.cli_flags["send-address"] = send_address
                contribution.cli_flags["send-role"] = "default"
            # For non-split slot (direction=None), also provide recv address (listener loads connector's repo)
            if direction is None:
                recv_repo_id = _deterministic_repo_id(scenario_id, "connector", "send")
                recv_address = _make_address(username, recv_repo_id)
                contribution.cli_flags["recv-address"] = recv_address
                contribution.cli_flags["recv-role"] = "default"
            elif direction == "recv":
                # Listener loads connector's SEND repo (upstream: connector->listener)
                # But this is handled by twoSixIndirectComposition adapter, not us
                pass

    elif request.role == "connector":
        if mode == "send":
            # Send-only: Connector is SENDER (creator) - only send direction
            if direction in (None, "send"):
                send_repo_id = _deterministic_repo_id(scenario_id, "connector", "send")
                send_address = _make_address(username, send_repo_id)
                contribution.address_output = send_address
                contribution.cli_flags["send-address"] = send_address
                contribution.cli_flags["send-role"] = "default"

        elif mode == "send-recv":
            # Send-recv: Connector creates its SEND repo, loads listener's SEND repo
            if direction in (None, "send"):
                send_repo_id = _deterministic_repo_id(scenario_id, "connector", "send")
                send_address = _make_address(username, send_repo_id)
                contribution.address_output = send_address
                contribution.cli_flags["send-address"] = send_address
                contribution.cli_flags["send-role"] = "default"
            if direction in (None, "recv"):
                recv_repo_id = _deterministic_repo_id(scenario_id, "listener", "send")
                recv_address = _make_address(username, recv_repo_id)
                contribution.cli_flags["recv-address"] = recv_address
                contribution.cli_flags["recv-role"] = "default"

        elif mode == "client-connect":
            # Client-connect: Connector is client side
            # For split-direction: recv=hfTransport, send=twoSixIndirect
            # If this adapter is called for "recv" direction (hfTransport):
            if direction in (None, "recv"):
                # Connector loads listener's SEND repo (downstream: listener->connector)
                recv_repo_id = _deterministic_repo_id(scenario_id, "listener", "send")
                recv_address = _make_address(username, recv_repo_id)
                contribution.address_output = recv_address
                contribution.cli_flags["recv-address"] = recv_address
                contribution.cli_flags["recv-role"] = "default"
            # For non-split slot (direction=None), also provide send address (connector creates its own repo)
            if direction is None:
                send_repo_id = _deterministic_repo_id(scenario_id, "connector", "send")
                send_address = _make_address(username, send_repo_id)
                contribution.cli_flags["send-address"] = send_address
                contribution.cli_flags["send-role"] = "default"
            elif direction == "send":
                # Connector creates its SEND repo (upstream: connector->listener)
                # But this is handled by twoSixIndirectComposition adapter, not us
                pass

    else:
        raise ValueError(f"HF Transport adapter: unknown role '{request.role}'")

    # Pass HF credentials via params (injected at runtime)
    # File-based user model action file depends on scenario mode
    # For send-only mode, use POST-only actions; for others, use default (POST + FETCH)
    if mode == "send" and request.role == "connector":
        action_file = "/kits/PluginHFTransport/config/actions-send-only.json"
    elif mode == "send":
        action_file = "config/actions.json"
    else:
        action_file = "/kits/PluginHFTransport/config/actions-combined.json"
    
    composition_name = composition or DEFAULT_COMPOSITION
    contribution.params.update({
        "PluginHFTransport.hf_username": "${HF_USERNAME}",
        "PluginHFTransport.hf_token": "${HF_TOKEN}",
        "PluginHFTransport.carrier_model": "${CARRIER_MODEL:-bert-base-uncased}",
        f"{composition_name}.actionFile": action_file,
    })
    # Also set as container environment variables for plugin to read directly
    contribution.environment.update({
        "HF_USERNAME": "${HF_USERNAME}",
        "HF_TOKEN": "${HF_TOKEN}",
        "CARRIER_MODEL": "${CARRIER_MODEL:-bert-base-uncased}",
    })

    return contribution