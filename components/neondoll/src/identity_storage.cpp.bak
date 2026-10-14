// Copyright 2026 Neon-Dolls/neondoll-embedded
// SPDX-License-Identifier: Apache-2.0

#include <neondoll/identity_storage.hpp>
#include <neondoll/identity.hpp>
#include <sstream>
#include <iomanip>
#include <vector>

namespace neondoll {

// Helper function to serialize IdentityState to JSON-like string
// Format: {"version":<int>,"identity":{"body_id":"<string>","name":"<string>","meta":{"implementation":"<string>","platform":"<string>","arch":"<string>"}}}
static std::string serialize_identity(const IdentityState& state) {
    std::ostringstream oss;
    oss << "{\"version\":" << state.version << ",\"identity\":{";
    oss << "\"body_id\":\"" << state.identity.body_id << "\",";
    oss << "\"name\":\"" << state.identity.name << "\",";
    oss << "\"meta\":{";
    oss << "\"implementation\":\"" << state.identity.meta.implementation << "\",";
    oss << "\"platform\":\"" << state.identity.meta.platform << "\",";
    oss << "\"arch\":\"" << state.identity.meta.arch << "\"";
    oss << "}}";
    oss << "}";
    return oss.str();
}

// Helper function to parse JSON-like string to IdentityState
// Expected format: {"version":<int>,"identity":{"body_id":"<string>","name":"<string>","meta":{"implementation":"<string>","platform":"<string>","arch":"<string>"}}}
static bool parse_identity(const std::string& json, IdentityState& out) {
    // Simple parser for our specific format
    // This is intentionally limited to avoid pulling in JSON library dependencies
    // In production, a proper JSON library like nlohmann/json or minimal JSON parser should be used
    
    size_t pos = 0;
    
    // Find version
    pos = json.find("\"version\":");
    if (pos == std::string::npos) return false;
    pos += 10; // length of "\"version\":"
    size_t end_pos = json.find(",", pos);
    if (end_pos == std::string::npos) end_pos = json.find("}", pos);
    if (end_pos == std::string::npos) return false;
    std::string version_str = json.substr(pos, end_pos - pos);
    try {
        out.version = std::stoi(version_str);
    } catch (...) {
        return false;
    }
    
    // Find body_id
    pos = json.find("\"body_id\":\"");
    if (pos == std::string::npos) return false;
    pos += 11; // length of "\"body_id\":\""
    end_pos = json.find("\"", pos);
    if (end_pos == std::string::npos) return false;
    out.identity.body_id = json.substr(pos, end_pos - pos);
    
    // Find name
    pos = json.find("\"name\":\"");
    if (pos != std::string::npos) {
        pos += 8; // length of "\"name\":\""
        end_pos = json.find("\"", pos);
        if (end_pos == std::string::npos) return false;
        out.identity.name = json.substr(pos, end_pos - pos);
    } else {
        out.identity.name = ""; // default empty if not present
    }
    
    // Find meta fields
    pos = json.find("\"implementation\":\"");
    if (pos == std::string::npos) return false;
    pos += 19; // length of "\"implementation\":\""
    end_pos = json.find("\"", pos);
    if (end_pos == std::string::npos) return false;
    out.identity.meta.implementation = json.substr(pos, end_pos - pos);
    
    pos = json.find("\"platform\":\"");
    if (pos == std::string::npos) return false;
    pos += 12; // length of "\"platform\":\""
    end_pos = json.find("\"", pos);
    if (end_pos == std::string::npos) return false;
    out.identity.meta.platform = json.substr(pos, end_pos - pos);
    
    pos = json.find("\"arch\":\"");
    if (pos == std::string::npos) return false;
    pos += 8; // length of "\"arch\":\""
    end_pos = json.find("\"", pos);
    if (end_pos == std::string::npos) return false;
    out.identity.meta.arch = json.substr(pos, end_pos - pos);
    
    return true;
}

int load_identity(Storage* storage, 
                 const std::string& key,
                 IdentityState& out) {
    if (!storage) {
        return -1; // null storage pointer
    }
    
    // Initialize storage
    int ret = storage->init();
    if (ret != 0) {
        return -2; // storage init failed
    }
    
    // First, check if the key exists by trying to read with a small buffer
    char test_buf[1];
    std::ptrdiff_t read_result = storage->read(key.c_str(), test_buf, sizeof(test_buf));
    if (read_result == -1) {
        storage->deinit();
        return -3; // key not found
    }
    if (read_result < 0) {
        storage->deinit();
        return -6; // storage read I/O error (other than -1)
    }
    
    // Determine the size needed
    // We'll read in chunks to avoid buffer size limitations
    constexpr size_t CHUNK_SIZE = 256;
    std::string json_data;
    json_data.reserve(1024); // reasonable initial capacity
    
    size_t offset = 0;
    while (true) {
        std::vector<char> chunk(CHUNK_SIZE);
        std::ptrdiff_t bytes_read = storage->read(key.c_str(), chunk.data(), CHUNK_SIZE);
        if (bytes_read == -1) {
            // key not found (shouldn't happen since we checked above, but handle anyway)
            storage->deinit();
            return -3;
        }
        if (bytes_read < 0) {
            storage->deinit();
            return -6; // storage read I/O error
        }
        if (bytes_read == 0) {
            // end of data
            break;
        }
        
        json_data.append(chunk.data(), bytes_read);
        offset += bytes_read;
        
        // If we got less than a full chunk, we're done
        if (static_cast<size_t>(bytes_read) < CHUNK_SIZE) {
            break;
        }
    }
    
    storage->deinit();
    
    // Parse the JSON data
    if (!parse_identity(json_data, out)) {
        return -4; // persisted data is corrupt or incomplete
    }
    
    // Check version compatibility
    if (out.version != CURRENT_IDENTITY_VERSION) {
        return -5; // persisted data has incompatible version
    }
    
    return 0; // success
}

int save_identity(Storage* storage,
                 const std::string& key,
                 const IdentityState& identity) {
    if (!storage) {
        return -1; // null storage pointer
    }
    
    // Initialize storage
    int ret = storage->init();
    if (ret != 0) {
        return -2; // storage init failed
    }
    
    // Serialize identity to JSON string
    std::string json_data = serialize_identity(identity);
    
    // Write the data
    ret = storage->write(key.c_str(), json_data.data(), json_data.size());
    if (ret != 0) {
        storage->deinit();
        return -3; // storage write I/O error
    }
    
    storage->deinit();
    return 0; // success
}

} // namespace neondoll