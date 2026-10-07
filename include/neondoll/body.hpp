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
    BodyImpl();
    bool init() override;
    std::shared_ptr<Identity> identity() const override;

private:
    std::shared_ptr<Identity> identity_;
};

} // namespace neondoll