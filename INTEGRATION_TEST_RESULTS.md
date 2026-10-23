# Integration Test Results — HF Transport

**Date:** 2026-10-08  
**Version:** 1.0.0

---

## Test Execution Log

### Test 8a: Unidirectional Send (`hf-send-only`)

**Command:**
```bash
HF_USERNAME=testuser HF_TOKEN=<token> python3 run_scenario.py --scenario-id hf-send-only --image-tag latest --adapter /home/ubuntu/racegen/hf-stego-transport/test/adapter.py
```

**Validation Checklist:**

| Check | Status | Notes |
|-------|--------|-------|
| **9.1 Config Correctness** | ☐ PASS / ☐ FAIL | Directionalities match (`LD_UNI`, `TT_UNICAST`), address parses as `hf://...` |
| **9.2 Plugin Loading** | ☐ PASS / ☐ FAIL | No errors before link creation; kits copied; `actions.json` loaded; HF credentials injected |
| **9.3 Runtime Execution** | ☐ PASS / ☐ FAIL | Link status → CREATED; HF create/upload works; repo created on HF Hub |
| **9.4 Test Completion** | ☐ PASS / ☐ FAIL | Test exits cleanly; link destroyed; repo deleted |

**Logs Location:** `/home/ubuntu/racegen/code/raceboat/test/integration/generated/hf-send-only/logs/`

**Result:** ☐ PASS / ☐ FAIL

---

### Test 8b: Send-Receive (`hf-send-recv`)

**Command:**
```bash
HF_USERNAME=testuser HF_TOKEN=<token> python3 run_scenario.py --scenario-id hf-send-recv --image-tag latest --adapter /home/ubuntu/racegen/hf-stego-transport/test/adapter.py
```

**Validation Checklist:**

| Check | Status | Notes |
|-------|--------|-------|
| **9.1 Config Correctness** | ☐ PASS / ☐ FAIL | Directionalities match, send/recv roles correct, address parses |
| **9.2 Plugin Loading** | ☐ PASS / ☐ FAIL | No errors before link creation; kits copied; `actions.json` loaded |
| **9.3 Runtime Execution** | ☐ PASS / ☐ FAIL | Link CREATED/LOADED; HF create/upload/download works; `onReceive` called with matching data |
| **9.4 Test Completion** | ☐ PASS / ☐ FAIL | Test exits cleanly; link destroyed; repo deleted (sender side) |

**Logs Location:** `/home/ubuntu/racegen/code/raceboat/test/integration/generated/hf-send-recv/logs/`

**Result:** ☐ PASS / ☐ FAIL

---

### Test 8c: Client-Connect (`hf-client-connect`)

**Command:**
```bash
HF_USERNAME=testuser HF_TOKEN=<token> python3 run_scenario.py --scenario-id hf-client-connect --image-tag latest --adapter /home/ubuntu/racegen/hf-stego-transport/test/adapter.py
```

**Validation Checklist:**

| Check | Status | Notes |
|-------|--------|-------|
| **9.1 Config Correctness** | ☐ PASS / ☐ FAIL | Directionalities match, two unidirectional links, addresses exchanged |
| **9.2 Plugin Loading** | ☐ PASS / ☐ FAIL | No errors before link creation; kits copied; `actions.json` loaded |
| **9.3 Runtime Execution** | ☐ PASS / ☐ FAIL | Both directions work; POST→FETCH cycles complete; data received in both directions |
| **9.4 Test Completion** | ☐ PASS / ☐ FAIL | Test exits cleanly; both links destroyed; both repos deleted |

**Logs Location:** `/home/ubuntu/racegen/code/raceboat/test/integration/generated/hf-client-connect/logs/`

**Result:** ☐ PASS / ☐ FAIL

---

## Summary

| Test | Status | Notes |
|------|--------|-------|
| 8a. Unidirectional Send | ☐ PASS / ☐ FAIL | |
| 8b. Send-Receive | ☐ PASS / ☐ FAIL | |
| 8c. Client-Connect | ☐ PASS / ☐ FAIL | |

**Overall:** ☐ ALL PASS / ☐ FAILURES

---

## Issues Encountered

| Test | Issue | Resolution |
|------|-------|------------|
| | | |

---

## HF Hub Verification

After each test, verify on Hugging Face Hub:
- [ ] Repo created: `https://huggingface.co/{username}/transport-{uuid}`
- [ ] Commits visible with expected messages
- [ ] Repo deleted after cleanup (sender side)
- [ ] No orphaned repos remaining