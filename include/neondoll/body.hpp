#pragma once
#include <string>
#include <memory>

namespace neondoll {

class Identity {
public:
    virtual ~Identity() = default;
    virtual std::uint32_t body_id() const = 0;
    virtual std::string to_string() const = 0;
};

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
