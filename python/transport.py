#!/usr/bin/env python3
"""
HF Stego Transport - Python Transport Component
Implements ITransportComponent for Hugging Face Hub steganography transport.
"""

import sys
import os
import json
import uuid
import time
import threading
from typing import Dict, List, Optional, Any
from dataclasses import dataclass, field
from enum import Enum

# Add paths for raceboat bindings
sys.path.append("/opt/raceboat/lib/python")
sys.path.append("/home/ubuntu/racegen/code/raceboat/language-shims/source/include")

try:
    from commsPluginBindings import (
        ITransportComponent,
        TransportProperties,
        LinkProperties,
        LinkPropertySet,
        LinkPropertyPair,
        LinkParameters,
        EncodingParameters,
        Action,
        RaceHandle,
        COMPONENT_OK,
        COMPONENT_ERROR,
        LT_BIDI,
        TT_INDIRECT,
        CT_INDIRECT,
        ST_MULTICAST,
        LINK_CREATED,
        LINK_LOADED,
        LINK_DESTROYED,
        PACKAGE_SENT,
        PACKAGE_FAILED_GENERIC,
    )
except ImportError as e:
    print(f"Warning: Could not import commsPluginBindings: {e}")
    # Mock for development
    class MockSDK:
        def __init__(self):
            self.handles = {}
        def onLinkStatusChanged(self, handle, linkId, status, params=None):
            print(f"MockSDK: onLinkStatusChanged handle={handle} linkId={linkId} status={status}")
        def onPackageStatusChanged(self, handle, status):
            print(f"MockSDK: onPackageStatusChanged handle={handle} status={status}")
        def onTimelineUpdated(self):
            print("MockSDK: onTimelineUpdated")
        def requestPluginUserInput(self, handle, key, prompt, required):
            print(f"MockSDK: requestPluginUserInput key={key} prompt={prompt}")
        def updateState(self, state):
            print(f"MockSDK: updateState state={state}")
        def getChannelProperties(self):
            class ChannelProps:
                maxLinks = 10
                currentRole = type('Role', (), {'linkSide': 1, 'roleName': 'test'})()
                transmissionType = TT_INDIRECT
                connectionType = CT_INDIRECT
                sendType = ST_MULTICAST
                reliable = True
                isFlushable = False
                duration_s = 86400
                period_s = 3600
                mtu = 50 * 1024 * 1024
                creatorExpected = type('LP', (), {'bandwidth_bps': 1000000, 'latency_ms': 5000, 'loss': 0.01})()
                supportedHints = []
                channelGid = "hfStegoTransport"
            return ChannelProps()
        def getActivePersona(self):
            return "testPersona"
    ITransportComponent = object
    TransportProperties = lambda: type('TP', (), {'supportedActions': {}})()
    LinkProperties = lambda: type('LP', (), {})()
    LinkPropertySet = lambda: type('LPS', (), {})()
    LinkPropertyPair = lambda: type('LPP', (), {})()
    LinkParameters = lambda: type('LP', (), {'json': ''})()
    EncodingParameters = lambda: type('EP', (), {'linkId': '', 'type': '', 'encodePackage': False, 'json': ''})()
    Action = lambda: type('Action', (), {'timestamp': 0.0, 'actionId': 0, 'json': ''})()
    RaceHandle = int
    COMPONENT_OK = 0
    COMPONENT_ERROR = -1
    LT_BIDI = 2
    TT_INDIRECT = 3
    CT_INDIRECT = 4
    ST_MULTICAST = 5
    LINK_CREATED = 1
    LINK_LOADED = 2
    LINK_DESTROYED = 3
    PACKAGE_SENT = 1
    PACKAGE_FAILED_GENERIC = -1

# Import our HF stub
sys.path.append("/home/ubuntu/racegen/hf_transport")
from stub import HFTransportStub


class LinkDirection(Enum):
    SEND = 1
    RECEIVE = 2
    BOTH = 3


@dataclass
class LinkAddress:
    username: str
    repo_name: str
    commit_sha: str = ""
    carrier_model: str = ""

    def to_string(self) -> str:
        addr = f"hf://{self.username}/{self.repo_name}"
        if self.commit_sha:
            addr += f"@{self.commit_sha}"
        return addr

    @staticmethod
    def from_string(address: str) -> Optional['LinkAddress']:
        if not address.startswith("hf://"):
            return None
        path = address[5:]
        commit_sha = ""
        if '@' in path:
            commit_sha, path = path.split('@', 1)
        if '/' not in path:
            return None
        username, repo_name = path.split('/', 1)
        return LinkAddress(username, repo_name, commit_sha)

    def get_repo_id(self) -> str:
        return f"{self.username}/{self.repo_name}"

    def to_json(self) -> dict:
        return {
            "username": self.username,
            "repo_name": self.repo_name,
            "commit_sha": self.commit_sha,
            "carrier_model": self.carrier_model
        }

    @staticmethod
    def from_json(data: dict) -> 'LinkAddress':
        return LinkAddress(
            data.get("username", ""),
            data.get("repo_name", ""),
            data.get("commit_sha", ""),
            data.get("carrier_model", "")
        )


@dataclass
class LinkState:
    link_id: str
    address: LinkAddress
    direction: LinkDirection
    active: bool = True
    last_commit_sha: str = ""
    created_at: float = field(default_factory=time.time)
    last_activity: float = field(default_factory=time.time)
    fetch_attempts: int = 0
    content_queue: Dict[int, bytes] = field(default_factory=dict)
    pending_payload: bytes = b""
    has_pending_payload: bool = False


class Link:
    """A single link instance for HF Stego Transport."""

    def __init__(self, link_id: str, address: LinkAddress, properties: LinkProperties,
                 direction: LinkDirection, sdk, hf_stub: HFTransportStub):
        self.sdk = sdk
        self.hf_stub = hf_stub
        self.link_id = link_id
        self.address = address
        self.properties = properties
        self.direction = direction
        self.state = LinkState(link_id, address, direction)

    def enqueue_content(self, action_id: int, content: bytes) -> int:
        self.state.content_queue[action_id] = content
        return COMPONENT_OK

    def dequeue_content(self, action_id: int) -> int:
        self.state.content_queue.pop(action_id, None)
        return COMPONENT_OK

    def get_content(self, action_id: int) -> Optional[bytes]:
        return self.state.content_queue.get(action_id)

    def set_pending_payload(self, payload: bytes):
        self.state.pending_payload = payload
        self.state.has_pending_payload = True

    def take_pending_payload(self) -> bytes:
        payload = self.state.pending_payload
        self.state.pending_payload = b""
        self.state.has_pending_payload = False
        return payload

    def post(self, handles: List[RaceHandle], action_id: int) -> int:
        content = self.get_content(action_id)
        if content is None:
            for handle in handles:
                self.sdk.onPackageStatusChanged(handle, PACKAGE_FAILED_GENERIC)
            return COMPONENT_ERROR

        try:
            # Upload to HF Hub
            import tempfile
            import shutil
            temp_dir = tempfile.mkdtemp(prefix=f"hf_stego_upload_{self.link_id}_")
            model_file = os.path.join(temp_dir, "model.safetensors")
            with open(model_file, "wb") as f:
                f.write(content)

            result = self.hf_stub.upload_folder(temp_dir, self.address.get_repo_id(),
                                                "racegen: post stego model")

            self.state.last_commit_sha = result.commit_sha
            self.state.last_activity = time.time()

            shutil.rmtree(temp_dir, ignore_errors=True)

            for handle in handles:
                self.sdk.onPackageStatusChanged(handle, PACKAGE_SENT)
            return COMPONENT_OK

        except Exception as e:
            print(f"Link {self.link_id} POST failed: {e}")
            for handle in handles:
                self.sdk.onPackageStatusChanged(handle, PACKAGE_FAILED_GENERIC)
            return COMPONENT_ERROR

    def fetch(self) -> int:
        try:
            import tempfile
            import shutil
            temp_dir = tempfile.mkdtemp(prefix=f"hf_stego_download_{self.link_id}_")

            result = self.hf_stub.download_repo(self.address.get_repo_id(), temp_dir)
            self.state.last_commit_sha = result.commit_sha
            self.state.last_activity = time.time()

            model_file = os.path.join(result.local_path, "model.safetensors")
            if os.path.exists(model_file):
                with open(model_file, "rb") as f:
                    model_data = f.read()

                # Pass to encoding module via SDK
                self.sdk.onReceive(self.link_id, EncodingParameters(), model_data)

            shutil.rmtree(temp_dir, ignore_errors=True)
            return COMPONENT_OK

        except HFTransportStub.NotFoundError:
            print(f"Link {self.link_id}: Repo not found (may have been deleted)")
            return COMPONENT_ERROR
        except Exception as e:
            print(f"Link {self.link_id} FETCH failed: {e}")
            return COMPONENT_ERROR

    def cleanup(self) -> int:
        if self.direction in (LinkDirection.SEND, LinkDirection.BOTH):
            try:
                self.hf_stub.delete_repo(self.address.get_repo_id())
                return COMPONENT_OK
            except Exception as e:
                print(f"Link {self.link_id} CLEANUP failed: {e}")
                return COMPONENT_ERROR
        return COMPONENT_OK


class PluginHFTransport(ITransportComponent):
    """HF Stego Transport Component for raceboat."""

    def __init__(self, sdk):
        super().__init__()
        self.sdk = sdk
        self.hf_stub = HFTransportStub()
        self.hf_username = ""
        self.hf_token = ""
        self.carrier_model = "bert-base-uncased"
        self.links: Dict[str, Link] = {}
        self.deleted_links: set = set()
        self.action_to_link: Dict[int, str] = {}
        self.next_action_id = 1
        self.next_hashtag = 0
        self.persona = sdk.getActivePersona()

        # Request user input for credentials
        sdk.requestPluginUserInput(0, "hf_username", "Enter Hugging Face username:", True)

    def onUserInputReceived(self, handle: RaceHandle, answered: bool, response: str) -> int:
        if not answered or not response:
            print(f"User input not answered or empty")
            return COMPONENT_ERROR

        if not self.hf_username:
            self.hf_username = response
            self.hf_stub.set_username(self.hf_username)
            self.sdk.requestPluginUserInput(handle, "hf_token",
                "Enter Hugging Face Personal Access Token (write scope):", True)
            return COMPONENT_OK

        if not self.hf_token:
            self.hf_token = response
            self.hf_stub.set_token(self.hf_token)
            self.sdk.requestPluginUserInput(handle, "carrier_model",
                "Enter carrier model ID (default: bert-base-uncased):", False)
            return COMPONENT_OK

        if not self.carrier_model or self.carrier_model == "bert-base-uncased":
            self.carrier_model = response if response else "bert-base-uncased"
            self.sdk.updateState(1)  # COMPONENT_STATE_STARTED
            return COMPONENT_OK

        return COMPONENT_OK

    def getTransportProperties(self) -> TransportProperties:
        props = TransportProperties()
        props.supportedActions = {
            "post": ["application/vnd.racegen.stego-model", "application/json"],
            "fetch": ["application/vnd.racegen.stego-model"],
            "cleanup": ["application/json"]
        }
        return props

    def getLinkProperties(self, link_id: str) -> LinkProperties:
        if link_id in self.links:
            return self.links[link_id].properties
        return self._default_link_properties()

    def _default_link_properties(self) -> LinkProperties:
        props = LinkProperties()
        props.linkType = LT_BIDI
        props.transmissionType = TT_INDIRECT
        props.connectionType = CT_INDIRECT
        props.sendType = ST_MULTICAST
        props.reliable = True
        props.isFlushable = False
        props.duration_s = 86400
        props.period_s = 3600
        props.mtu = 50 * 1024 * 1024

        # Set bandwidth/latency/loss
        for prop_set in [props.worst, props.expected, props.best]:
            prop_set.send.bandwidth_bps = 1000000
            prop_set.send.latency_ms = 5000
            prop_set.send.loss = 0.01
            prop_set.receive.bandwidth_bps = 1000000
            prop_set.receive.latency_ms = 5000
            prop_set.receive.loss = 0.01

        props.supportedHints = []
        props.channelGid = "hfStegoTransport"
        return props

    def createLink(self, handle: RaceHandle, link_id: str) -> int:
        if len(self.links) >= self.sdk.getChannelProperties().maxLinks:
            self.sdk.onLinkStatusChanged(handle, link_id, LINK_DESTROYED)
            return COMPONENT_ERROR

        hashtag = f"hfstego_{self.persona}_{self.next_hashtag}"
        self.next_hashtag += 1

        address = LinkAddress(
            username=self.hf_username,
            repo_name=f"transport-{hashtag}",
            carrier_model=self.carrier_model
        )

        properties = self._default_link_properties()
        properties.linkAddress = address.to_string()

        link = Link(link_id, address, properties, LinkDirection.SEND, self.sdk, self.hf_stub)
        self.links[link_id] = link
        self.deleted_links.discard(link_id)

        self.sdk.onLinkStatusChanged(handle, link_id, LINK_CREATED)
        self.sdk.onTimelineUpdated()

        return COMPONENT_OK

    def loadLinkAddress(self, handle: RaceHandle, link_id: str, link_address: str) -> int:
        address = LinkAddress.from_string(link_address)
        if not address:
            self.sdk.onLinkStatusChanged(handle, link_id, LINK_DESTROYED)
            return COMPONENT_ERROR

        # Verify repo exists
        if not self.hf_stub.repo_exists(address.get_repo_id()):
            self.sdk.onLinkStatusChanged(handle, link_id, LINK_DESTROYED)
            return COMPONENT_ERROR

        properties = self._default_link_properties()
        properties.linkAddress = address.to_string()

        link = Link(link_id, address, properties, LinkDirection.RECEIVE, self.sdk, self.hf_stub)
        self.links[link_id] = link
        self.deleted_links.discard(link_id)

        self.sdk.onLinkStatusChanged(handle, link_id, LINK_LOADED)
        self.sdk.onTimelineUpdated()

        return COMPONENT_OK

    def loadLinkAddresses(self, handle: RaceHandle, link_id: str, link_addresses: List[str]) -> int:
        # Not supported - single address per link
        self.sdk.onLinkStatusChanged(handle, link_id, LINK_DESTROYED)
        return COMPONENT_ERROR

    def createLinkFromAddress(self, handle: RaceHandle, link_id: str, link_address: str) -> int:
        return self.loadLinkAddress(handle, link_id, link_address)

    def destroyLink(self, handle: RaceHandle, link_id: str) -> int:
        if link_id not in self.links:
            return COMPONENT_ERROR

        link = self.links.pop(link_id)
        self.deleted_links.add(link_id)

        # If sender, delete repo
        if link.direction in (LinkDirection.SEND, LinkDirection.BOTH):
            try:
                self.hf_stub.delete_repo(link.address.get_repo_id())
            except Exception as e:
                print(f"Error deleting repo: {e}")

        self.sdk.onLinkStatusChanged(handle, link_id, LINK_DESTROYED)
        self.sdk.onTimelineUpdated()

        return COMPONENT_OK

    def _parse_action_json(self, json_str: str) -> Optional[dict]:
        try:
            return json.loads(json_str)
        except Exception as e:
            print(f"Failed to parse action JSON: {e}")
            return None

    def getActionParams(self, action: Action) -> List[EncodingParameters]:
        action_data = self._parse_action_json(action.json)
        if not action_data:
            return []

        link_id = action_data.get("linkId", "")
        action_type = action_data.get("type", "")

        params = EncodingParameters()
        params.linkId = link_id

        if action_type == "post":
            params.type = "application/vnd.racegen.stego-model"
            params.encodePackage = True
            params.json = json.dumps({
                "carrier_model": self.carrier_model,
                "action_type": "post",
                "linkId": link_id
            })
        elif action_type == "fetch":
            params.type = "application/vnd.racegen.stego-model"
            params.encodePackage = False
            params.json = json.dumps({
                "action_type": "fetch",
                "linkId": link_id
            })
        elif action_type == "cleanup":
            params.type = "application/json"
            params.encodePackage = False
            params.json = json.dumps({
                "action_type": "cleanup",
                "linkId": link_id
            })
        else:
            return []

        return [params]

    def enqueueContent(self, params: EncodingParameters, action: Action, content: bytes) -> int:
        self.action_to_link[action.actionId] = params.linkId

        if params.linkId in self.deleted_links:
            return COMPONENT_OK

        if params.linkId not in self.links:
            return COMPONENT_ERROR

        link = self.links[params.linkId]
        link.enqueue_content(action.actionId, content)

        # For POST, also store as pending
        action_data = self._parse_action_json(action.json)
        if action_data and action_data.get("type") == "post":
            link.set_pending_payload(content)

        return COMPONENT_OK

    def dequeueContent(self, action: Action) -> int:
        action_data = self._parse_action_json(action.json)
        link_id = action_data.get("linkId") if action_data else None

        if not link_id:
            link_id = self.action_to_link.get(action.actionId)

        if not link_id:
            return COMPONENT_OK

        if link_id in self.deleted_links:
            self.action_to_link.pop(action.actionId, None)
            return COMPONENT_OK

        if link_id in self.links:
            self.links[link_id].dequeue_content(action.actionId)

        self.action_to_link.pop(action.actionId, None)
        return COMPONENT_OK

    def doAction(self, handles: List[RaceHandle], action: Action) -> int:
        action_data = self._parse_action_json(action.json)
        if not action_data:
            for handle in handles:
                self.sdk.onPackageStatusChanged(handle, PACKAGE_FAILED_GENERIC)
            return COMPONENT_ERROR

        link_id = action_data.get("linkId", "")
        action_type = action_data.get("type", "")

        if link_id in self.deleted_links:
            for handle in handles:
                self.sdk.onPackageStatusChanged(handle, PACKAGE_FAILED_GENERIC)
            return COMPONENT_OK

        if link_id not in self.links:
            for handle in handles:
                self.sdk.onPackageStatusChanged(handle, PACKAGE_FAILED_GENERIC)
            return COMPONENT_ERROR

        link = self.links[link_id]

        if action_type == "post":
            return link.post(handles, action.actionId)
        elif action_type == "fetch":
            return link.fetch()
        elif action_type == "cleanup":
            return link.cleanup()
        else:
            for handle in handles:
                self.sdk.onPackageStatusChanged(handle, PACKAGE_FAILED_GENERIC)
            return COMPONENT_ERROR

    def onSendPackage(self, link_id: str, bytes_count: int) -> List[Action]:
        action = Action()
        action.timestamp = 0.0  # Execute immediately
        action.actionId = self.next_action_id
        self.next_action_id += 1
        action.json = json.dumps({
            "type": "post",
            "linkId": link_id,
            "payloadHash": ""
        })
        return [action]


def createTransport(name: str, sdk, role: str, plugin_config) -> PluginHFTransport:
    """Factory function for raceboat Python plugin loader."""
    return PluginHFTransport(sdk)