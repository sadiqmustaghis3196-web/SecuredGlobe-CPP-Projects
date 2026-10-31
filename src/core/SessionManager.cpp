#include "SessionManager.hpp"
#include <stdexcept>
void SessionManager::start(const User &u) {
    user_ = u;
    clock_.start();
}
void SessionManager::logout() {
    user_ = User{};
    clock_.invalidate();
}
bool SessionManager::active() const {
    return user_.id != 0 && !expired();
}
bool SessionManager::expired() const {
    return clock_.isValid() && clock_.elapsed() >= 15 * 60 * 1000;
}
void SessionManager::touch() {
    if (active())
        clock_.restart();
}
const User &SessionManager::user() const {
    if (!active())
        throw std::runtime_error("Session expired. Please sign in again");
    return user_;
}
void SessionManager::clearPasswordChange() {
    user_.mustChange = false;
}
