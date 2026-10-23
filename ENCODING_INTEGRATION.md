# Encoding Integration — HF Transport

**Date:** 2026-10-08

---

## Decision: Use `noop` Encoding

The HF Transport uses the existing **`noop` encoding** (`application/octet-stream`) from the decomposed-exemplars repository.

### Why `noop` Encoding?

1. **HF Hub accepts arbitrary binary** — `upload_folder` and `snapshot_download` work with any file format
2. **Transport is carrier-agnostic** — the steganography module (external) produces carrier files; transport just moves bytes
3. **No transformation needed** — `noop` encoding passes bytes through unchanged
4. **Matches transport MIME types** — `getTransportProperties()` declares `application/octet-stream` for both post and fetch

### Encoding Properties (from `PluginCommsTwoSixNoopEncoding`)

```cpp
EncodingProperties getEncodingProperties() {
    return {0, "application/octet-stream"};  // 0 = encodingTime, MIME type
}

SpecificEncodingProperties getEncodingPropertiesForParameters(const EncodingParameters&) {
    return {1000000};  // maxBytes = 1MB (conservative)
}

ComponentStatus encodeBytes(RaceHandle handle, const EncodingParameters& params, const std::vector<uint8_t>& bytes) {
    sdk->onBytesEncoded(handle, bytes, ENCODE_OK);  // Identity function
    return COMPONENT_OK;
}

ComponentStatus decodeBytes(RaceHandle handle, const EncodingParameters& params, const std::vector<uint8_t>& bytes) {
    sdk->onBytesDecoded(handle, bytes, ENCODE_OK);  // Identity function
    return COMPONENT_OK;
}
```

### Configuration

The manifest composition specifies:
```json
"compositions": [{
    "id": "hfTransportComposition",
    "transport": "hfTransport",
    "usermodel": "twoSixFileBasedUserModel",
    "encodings": ["noop"]
}]
```

### Future: Steganography Encoding

If steganography is needed later, a new encoding would be required with:
- **MIME type**: `application/vnd.racegen.stego-model`
- **Config parameters**: carrier_model, algorithm, bit_depth, layers, seed
- **Max payload size**: Depends on carrier model (e.g., bert-base-uncased ~400MB ≈ 50MB payload with LSB)

**But for now: STOP — use `noop` encoding.** The transport component works end-to-end with raw binary payloads.

---

## Verification

- ✅ `noop` encoding exists in `decomposed-exemplars/source/encoding/PluginCommsTwoSixNoopEncoding.h/cpp`
- ✅ MIME type `application/octet-stream` matches transport's `getTransportProperties()`
- ✅ No C++ encoding code needed — reuse existing plugin
- ✅ Manifest correctly references `encodings: ["noop"]`