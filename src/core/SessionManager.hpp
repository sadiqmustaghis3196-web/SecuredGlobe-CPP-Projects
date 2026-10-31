#pragma once
#include "models/User.hpp"
#include <QElapsedTimer>
class SessionManager final {
  public:
    void start(const User &user);
    void logout();
    bool active() const;
    bool expired() const;
    void touch();
    const User &user() const;
    void clearPasswordChange();

  private:
    User user_;
    QElapsedTimer clock_;
};
