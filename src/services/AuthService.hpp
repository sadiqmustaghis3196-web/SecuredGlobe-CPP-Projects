#pragma once
#include "core/SessionManager.hpp"
#include "repositories/BankRepository.hpp"
class AuthService final {
  public:
    AuthService(BankRepository &repository, SessionManager &session);
    void login(const QString &username, const QString &password);
    void logout();
    void changePassword(const QString &oldPassword, const QString &newPassword);
    void requestReset(const QString &username);
    void resetPassword(qint64 userId, const QString &temporaryPassword);
    qint64 createUser(const QString &username, const QString &name, const QString &role,
                      const QString &password, qint64 customerId = 0);
    void changeRole(qint64 userId, const QString &role);
    void setActive(qint64 userId, bool active);

  private:
    BankRepository &repo_;
    SessionManager &session_;
    void requireAdmin() const;
    void setPassword(qint64 userId, const QString &password, bool forceChange);
};
