#ifndef NEONDOLL_IDENTITY_HPP
#define NEONDOLL_IDENTITY_HPP

#include <cstdint>
#include <string>
#include <memory>

namespace neondoll {

class Identity {
public:
    virtual ~Identity() = default;
    virtual std::uint32_t body_id() const = 0;
    virtual std::string to_string() const = 0;
};

/**
 * @brief Minimal stub implementation for M0.
 * Returns a fixed body_id (0) and simple string.
 */
class IdentityImpl : public Identity {
public:
    std::uint32_t body_id() const override { return 0; }
    std::string to_string() const override { return "Stub Identity (M0)"; }
};

using IdentityPtr = std::shared_ptr<Identity>;

inline IdentityPtr create_identity() {
    return std::make_shared<IdentityImpl>();
}

} // namespace neondoll

#endif // NEONDOLL_IDENTITY_HPP
