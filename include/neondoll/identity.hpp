#pragma once
#include <string>
#include <memory>

namespace neondoll {

class Identity {
public:
    virtual ~Identity() = default;
    virtual std::string body_id() const = 0;
    virtual bool is_initialized() const = 0;
    virtual void set_initialized(bool initialized) = 0;
};

class IdentityImpl : public Identity {
public:
    IdentityImpl() : initialized_(false) {}

    std::string body_id() const override {
        return initialized_ ? "placeholder-body-id" : "";
    }

    bool is_initialized() const override {
        return initialized_;
    }

    void set_initialized(bool initialized) override {
        initialized_ = initialized;
    }

private:
    bool initialized_;
};

std::shared_ptr<Identity> create_identity() {
    return std::make_shared<IdentityImpl>();
}

} // namespace neondoll