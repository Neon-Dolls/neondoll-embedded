// Copyright 2026 Neon-Dolls/neondoll-embedded
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <neondoll/platform.hpp>
#include <neondoll/identity.hpp>
#include <neondoll/identity_storage.hpp>
#include <cstring>
#include <string>
#include <map>

// Mock platform objects for testing
namespace neondoll {

class MockStorage : public Storage {
public:
    MockStorage() : initialized_(false), fail_init_(false) {}
    virtual ~MockStorage() { if (initialized_) deinit(); }
    
    int init() override { 
        if (fail_init_) return -2; 
        initialized_ = true; 
        return 0; 
    }
    int deinit() override { initialized_ = false; return 0; }
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
        storage_[key] = std::string(static_cast<const char*>(in), size);
        return static_cast<int>(size);
    }
    
    void set_fail_init(bool fail) { fail_init_ = fail; }
    void clear() { storage_.clear(); }
    
private:
    bool initialized_;
    bool fail_init_;
    std::map<std::string, std::string> storage_;
};

class MockEntropy : public EntropySource {
public:
    MockEntropy() : initialized_(false), fail_init_(false), fail_random_(false) {}
    virtual ~MockEntropy() { if (initialized_) deinit(); }
    
    int init() override { 
        if (fail_init_) return -2; 
        initialized_ = true; 
        return 0; 
    }
    int deinit() override { initialized_ = false; return 0; }
    int get_random(void* out, size_t len) override {
        if (!initialized_) return -2;
        if (fail_random_) return -1;
        // Fill with predictable pattern for testing
        for (size_t i = 0; i < len; ++i) {
            static_cast<uint8_t*>(out)[i] = static_cast<uint8_t>(i);
        }
        return static_cast<int>(len);
    }
    
    void set_fail_init(bool fail) { fail_init_ = fail; }
    void set_fail_random(bool fail) { fail_random_ = fail; }
    void reset() { fail_init_ = false; fail_random_ = false; }
    
private:
    bool initialized_;
    bool fail_init_;
    bool fail_random_;
};

class MockClock : public Clock {
public:
    MockClock() : initialized_(false), now_ms_return_(0) {}
    virtual ~MockClock() { if (initialized_) deinit(); }
    int init() override { ++init_calls_; return init_return_; }
    int deinit() override { ++deinit_calls_; return deinit_return_; }
    uint64_t now_ms() override { return now_ms_return_; }
    
    int init_calls_ = 0;
    int deinit_calls_ = 0;
    int init_return_ = 0;
    int deinit_return_ = 0;
    uint64_t now_ms_return_;
    
private:
    bool initialized_;
};

