# Integration Test Results — HF Transport

**Date:** 2026-10-26 · **Images:** builder/runtime tag `hf-testing` · **Plugin:** `PluginHFTransport` (`libHFTransport.so`)

## Commands

```bash
cd /home/ubuntu/racegen/hf-stego-transport
./build_artifacts_in_docker_image.sh --image-tag hf-testing
./run_unit_tests.sh --image-tag hf-testing                                  # 26 tests, all passed
set -a; . ../.env; set +a                             # HF_USERNAME, HF_TOKEN (write-scope PAT)
# stale repos from earlier runs of the same scenario (deterministic names), one prefix per scenario:
python3 test/cleanup_hf_repos.py --prefix transport-hf_send_recv-      # --dry-run first
./test/run_scenarios.py --image-tag hf-testing
```

## Results

| Scenario | Result | What it shows | Preserved logs |
|---|---|---|---|
| `hf-send-only` | PASSED | connector POST creates its repo and uploads a commit; listener (loader of that repo) fetches and prints the message | `test/logs/20261026-165729-hf-send-only/` |
| `hf-send-recv` | PASSED | request and reply, each direction in its own repo (`…-connector-send`, `…-listener-send`) | `test/logs/20261026-165855-hf-send-recv/` |
| `hf-client-connect` | PASSED | persistent socket proxy, two-way traffic over the two repos | `test/logs/20261026-170020-hf-client-connect/` |
| `hf-mixed-client-connect` | PASSED | upstream over `twoSixIndirectComposition`, downstream over HF; adapter fills only the requested direction | `test/logs/20261026-170208-hf-mixed-client-connect/` |

Credentials reach the component only through `--param hfTransportComposition.hf_username|hf_token` (answers to `requestPluginUserInput`); the containers have no `HF_*` environment variables. The `getenv` fallback in `PluginHFTransport::handleUserInputResponse` exists only for unanswered parameters.

## Changes since the first synthesis (why the earlier "infrastructure bug" notes no longer apply)

- Credential params used the plugin name as prefix (`PluginHFTransport.hf_token`), which the SDK never delivers, so the plugin was fed through container environment variables. Params now use the composition id.
- The send-only listener's action file was the relative path `config/actions.json`, which only worked because the user model resolves relative paths against *its own* kit; all modes now use `/kits/PluginHFTransport/config/…`.
- `manifest.json` used `bandwidth`/`latency` (ignored by the parser) instead of `bandwidth_bps`/`latency_ms`; link properties now come from the manifest instead of hard-coded duplicates.
- Component not-ready path now reports `LINK_DESTROYED`; debug `fprintf` calls were removed from the component.
- The unit tests were stale and had never compiled (hand-written `MockTransportSdk` with wrong signatures, tests of the removed `hf://` address format). They now use the real mock and pass.
- `test/scenarios/*.json` replaced by the canonical scenario files; legacy `test/docker-compose.yml` removed.
- `HfTransportStub.cpp` still writes per-call debug files (`hf_debug.log`, `hf_*` directories next to the logs). Harmless for tests but noisy; remove when next touching it.

Log note: `race-cli --debug` prints every `--param` value, so the raw logs in `raceboat/test/integration/generated/` contain the token. The copies preserved by `run_scenarios.py` are masked.
