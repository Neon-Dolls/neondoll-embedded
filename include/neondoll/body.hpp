#pragma once
#include <string>
#include <memory>
#include "identity.hpp"

namespace neondoll {

class Body {
public:
    virtual ~Body() = default;
    virtual bool init() = 0;
    virtual std::shared_ptr<Identity> identity() const = 0;
};

class BodyImpl : public Body {
public:
    BodyImpl() : identity_(create_identity()) {}

    bool init() override {
        // For M0, we just set the identity as initialized.
        // In a real implementation, this would load or create the durable identity.
        identity_->set_initialized(true);
        return true;
    }

    std::shared_ptr<Identity> identity() const override {
        return identity_;
    }

private:
    std::shared_ptr<Identity> identity_;
};

} // namespace neondoll