#include <neondoll/body.hpp>
#include <neondoll/identity.hpp>

namespace neondoll {

BodyImpl::BodyImpl() : identity_(std::make_shared<IdentityImpl>()) {}

bool BodyImpl::init() {
    // For M0, we just set the identity as initialized.
    // In a real implementation, this would load or create the durable identity.
    identity_->set_initialized(true);
    return true;
}

std::shared_ptr<Identity> BodyImpl::identity() const {
    return identity_;
}

} // namespace neondoll