// Copyright 2026 Neon-Dolls/neondoll-embedded
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <neondoll/platform.hpp>
#include <neondoll/identity.hpp>
#include <neondoll/identity_storage.hpp>
#include <cstring>
#include <string>
#include <map>
#include <cstdio> // for fprintf

// Mock platform objects for testing
namespace neondoll {

class MockStorage : public Storage {
public:
    MockStorage() : initialized_(false), fail_init_(false) {}
    virtual ~MockStorage() { if (initialized_) deinit(); }

    int init() override { 
        if (fail_init_) {
            fprintf(stderr, "MockStorage::init: fail_init_ is true, returning -2\n");
            return -2; 
        }
        fprintf(stderr, "MockStorage::init: fail_init_ is false, setting initialized_=true and returning 0\n");
        initialized_ = true; 
        return 0; 
    }
    int deinit() override { 
        fprintf(stderr, "MockStorage::deinit: setting initialized_=false\n");
        initialized_ = false; 
        return 0; 
    }
    std::ptrdiff_t read(const char* key, void* out, size_t max_size) override {
        if (!initialized_) return -2;
        auto it = storage_.find(key);
        if (it == storage_.end()) return -1;
        size_t copy_len = std::min(max_size, it->second.size());
        memcpy(out, it->second.data(), copy_len);
        return static_cast<std::ptrdiff_t>(copy_len);
    }
    int write(const char* key, const void* in, size_t size) override {
        if (!initialized_) return -2;
        // For the StorageWriteFailure test, simulate a write failure
        if (fail_write_) {
            return -3; // I/O error
        }
        storage_[key] = std::string(static_cast<const char*>(in), size);
        return static_cast<int>(size);
    }

    void set_fail_init(bool fail) { fail_init_ = fail; }
    void set_fail_write(bool fail) { fail_write_ = fail; }
    void clear() { storage_.clear(); }

private:
    bool initialized_;
    bool fail_init_;
    bool fail_write_;
    std::map<std::string, std::string> storage_;
};

} // namespace neondoll