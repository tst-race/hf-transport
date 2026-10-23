#include "PersistentStorageHelpers.h"
#include <ITransportSdk.h>
#include <sstream>
#include <vector>

namespace psh {

bool saveValue(ITransportSdk* sdk, const std::string& key, const std::string& value) {
    try {
        // Convert key to a safe filename
        std::string filename = "persist/" + key;
        // Replace any path separators in key
        for (char& c : filename) {
            if (c == '/') c = '_';
        }
        
        std::vector<uint8_t> data(value.begin(), value.end());
        auto response = sdk->writeFile(filename, data);
        return response.status == CM_OK;
    } catch (...) {
        return false;
    }
}

std::string readValue(ITransportSdk* sdk, const std::string& key, const std::string& defaultValue) {
    try {
        std::string filename = "persist/" + key;
        for (char& c : filename) {
            if (c == '/') c = '_';
        }
        
        auto data = sdk->readFile(filename);
        if (data.empty()) {
            return defaultValue;
        }
        return std::string(data.begin(), data.end());
    } catch (...) {
        return defaultValue;
    }
}

bool deleteValue(ITransportSdk* sdk, const std::string& key) {
    try {
        std::string filename = "persist/" + key;
        for (char& c : filename) {
            if (c == '/') c = '_';
        }
        
        // writeFile with empty data effectively deletes
        std::vector<uint8_t> empty;
        auto response = sdk->writeFile(filename, empty);
        return response.status == CM_OK;
    } catch (...) {
        return false;
    }
}

} // namespace psh