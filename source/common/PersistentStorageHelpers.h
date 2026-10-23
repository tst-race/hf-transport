#ifndef PERSISTENT_STORAGE_HELPERS_H
#define PERSISTENT_STORAGE_HELPERS_H

#include <string>
#include <vector>
#include <ComponentTypes.h>
#include <ITransportSdk.h>

namespace psh {

// Save a value to persistent storage
bool saveValue(ITransportSdk* sdk, const std::string& key, const std::string& value);

// Load a value from persistent storage
std::string readValue(ITransportSdk* sdk, const std::string& key, const std::string& defaultValue = "");

// Delete a value from persistent storage
bool deleteValue(ITransportSdk* sdk, const std::string& key);

// HF-specific key prefixes
inline std::string cursorKey(const std::string& linkId) {
    return "hf:link:" + linkId + ":cursor";
}

inline std::string headKey(const std::string& linkId) {
    return "hf:link:" + linkId + ":head";
}

inline std::string addressKey(const std::string& linkId) {
    return "hf:link:" + linkId + ":address";
}

} // namespace psh

#endif // PERSISTENT_STORAGE_HELPERS_H