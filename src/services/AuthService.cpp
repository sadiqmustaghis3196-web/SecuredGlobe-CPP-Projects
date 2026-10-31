#include "AuthService.hpp"
#include "core/TransactionGuard.hpp"
#include "utils/PasswordHasher.hpp"
#include <QDateTime>
#include <QRegularExpression>
#include <stdexcept>
AuthService::AuthService(BankRepository &r, SessionManager &s) : repo_(r), session_(s) {}
void AuthService::login(const QString &username, const QString &password) {
    session_.logout();
    auto rows = repo_.records("SELECT u.*,r.name AS role FROM users u JOIN user_roles ur ON "
                              "ur.user_id=u.id JOIN roles r ON r.id=ur.role_id WHERE username=?",
                              {username.trimmed()});
    bool success = false;
    Record row;
    if (!rows.isEmpty()) {
        row = rows.first();
        if (row["locked_until"].toLongLong() > QDateTime::currentSecsSinceEpoch())
            throw std::runtime_error("Account locked. Try again after five minutes");
        success =
            row["active"].toBool() &&
            PasswordHasher::verify(password, row["salt"].toByteArray(),
                                   row["password_hash"].toByteArray(), row["iterations"].toInt());
    } else {
        PasswordHasher::hash(password, QByteArray(24, 'x'));
    }
    TransactionGuard tx(repo_.database().connection());
    repo_.execute("INSERT INTO login_attempts(username,successful) VALUES(?,?)",
                  {username.trimmed(), success ? 1 : 0});
    if (!success) {
        if (!row.isEmpty()) {
            int failures = row["failed_attempts"].toInt() + 1;
            if (row["locked_until"].toLongLong() > 0 &&
                row["locked_until"].toLongLong() <= QDateTime::currentSecsSinceEpoch())
                failures = 1;
            repo_.execute("UPDATE users SET failed_attempts=?,locked_until=? WHERE id=?",
                          {failures, failures >= 3 ? QDateTime::currentSecsSinceEpoch() + 300 : 0,
                           row["id"]});
        }
        tx.commit();
        throw std::runtime_error("Invalid username or password");
    }
    repo_.execute("UPDATE users SET failed_attempts=0,locked_until=0 WHERE id=?", {row["id"]});
    repo_.execute("INSERT INTO audit_logs(actor_id,action,detail) VALUES(?,'Login','Successful "
                  "authentication')",
                  {row["id"]});
    User u = User::fromRecord(row);
    for (const auto &p : repo_.records(
             "SELECT DISTINCT p.name FROM permissions p JOIN role_permissions rp ON "
             "rp.permission_id=p.id JOIN user_roles ur ON ur.role_id=rp.role_id WHERE ur.user_id=?",
             {u.id}))
        u.permissions << p["name"].toString();
    tx.commit();
    session_.start(u);
}
void AuthService::logout() {
    if (session_.active())
        repo_.execute(
            "INSERT INTO audit_logs(actor_id,action,detail) VALUES(?,'Logout','Session ended')",
            {session_.user().id});
    session_.logout();
}
void AuthService::setPassword(qint64 id, const QString &p, bool force) {
    PasswordHasher::validate(p);
    auto salt = PasswordHasher::salt();
    repo_.execute(
        "UPDATE users SET "
        "password_hash=?,salt=?,iterations=?,must_change=?,failed_attempts=0,locked_until=0 WHERE "
        "id=?",
        {PasswordHasher::hash(p, salt), salt, PasswordHasher::Iterations, force ? 1 : 0, id});
}
void AuthService::changePassword(const QString &old, const QString &next) {
    const auto u = session_.user();
    auto row = repo_.one("SELECT * FROM users WHERE id=?", {u.id});
    if (!PasswordHasher::verify(old, row["salt"].toByteArray(), row["password_hash"].toByteArray(),
                                row["iterations"].toInt()))
        throw std::runtime_error("Current password is incorrect");
    if (old == next)
        throw std::runtime_error("Choose a different password");
    TransactionGuard tx(repo_.database().connection());
    setPassword(u.id, next, false);
    repo_.execute("INSERT INTO audit_logs(actor_id,action,detail) VALUES(?,'Password "
                  "changed','Self-service password update')",
                  {u.id});
    tx.commit();
    session_.clearPasswordChange();
}
void AuthService::requestReset(const QString &username) {
    if (username.trimmed().size() < 2 || username.size() > 64)
        throw std::runtime_error("Enter a valid username");
    repo_.execute("INSERT INTO notifications(message) VALUES(?)",
                  {"Password reset requested for username: " + username.trimmed()});
}
void AuthService::requireAdmin() const {
    auto u = session_.user();
    auto live = repo_.one("SELECT active,must_change FROM users WHERE id=?", {u.id});
    if (!live["active"].toBool() || live["must_change"].toBool() || u.mustChange ||
        !u.can("admin") ||
        repo_.one("SELECT COUNT(*) AS n FROM user_roles ur JOIN roles r ON r.id=ur.role_id WHERE "
                  "ur.user_id=? AND r.name='Administrator'",
                  {u.id})["n"]
                .toInt() == 0)
        throw std::runtime_error("Administrator permission required");
}
void AuthService::resetPassword(qint64 id, const QString &password) {
    requireAdmin();
    repo_.one("SELECT id FROM users WHERE id=?", {id});
    TransactionGuard tx(repo_.database().connection());
    setPassword(id, password, true);
    repo_.execute(
        "INSERT INTO audit_logs(actor_id,action,detail) VALUES(?,'Admin password reset',?)",
        {session_.user().id, QString::number(id)});
    tx.commit();
}
qint64 AuthService::createUser(const QString &username, const QString &name, const QString &role,
                               const QString &password, qint64 customerId) {
    requireAdmin();
    PasswordHasher::validate(password);
    if (!QRegularExpression("^[A-Za-z0-9_.-]{3,40}$").match(username).hasMatch() ||
        name.trimmed().size() < 2)
        throw std::runtime_error("Invalid username or display name");
    auto r = repo_.one("SELECT id FROM roles WHERE name=?", {role});
    if (role == "Customer") {
        if (customerId <= 0)
            throw std::runtime_error("Select a customer for this login");
        repo_.one("SELECT id FROM customers WHERE id=?", {customerId});
    } else if (customerId != 0)
        throw std::runtime_error("Staff users cannot be linked to customer profiles");
    TransactionGuard tx(repo_.database().connection());
    auto salt = PasswordHasher::salt();
    qint64 id = repo_.execute(
        "INSERT INTO users(username,name,password_hash,salt,iterations,customer_id) "
        "VALUES(?,?,?,?,?,?)",
        {username.trimmed(), name.trimmed(), PasswordHasher::hash(password, salt), salt,
         PasswordHasher::Iterations, customerId ? QVariant(customerId) : QVariant()});
    repo_.execute("INSERT INTO user_roles(user_id,role_id) VALUES(?,?)", {id, r["id"]});
    repo_.execute("INSERT INTO audit_logs(actor_id,action,detail) VALUES(?,'User created',?)",
                  {session_.user().id, username});
    tx.commit();
    return id;
}
void AuthService::changeRole(qint64 id, const QString &role) {
    requireAdmin();
    if (id == session_.user().id)
        throw std::runtime_error("Cannot change your own role");
    auto u = repo_.one("SELECT customer_id FROM users WHERE id=?", {id});
    if (role == "Customer" && u["customer_id"].isNull())
        throw std::runtime_error("Customer role requires a linked customer profile");
    if (role != "Customer" && !u["customer_id"].isNull())
        throw std::runtime_error("Customer-linked users cannot become staff");
    auto r = repo_.one("SELECT id FROM roles WHERE name=?", {role});
    TransactionGuard tx(repo_.database().connection());
    repo_.execute("DELETE FROM user_roles WHERE user_id=?", {id});
    repo_.execute("INSERT INTO user_roles(user_id,role_id) VALUES(?,?)", {id, r["id"]});
    repo_.execute("INSERT INTO audit_logs(actor_id,action,detail) VALUES(?,'Role changed',?)",
                  {session_.user().id, QString::number(id) + " / " + role});
    tx.commit();
}
void AuthService::setActive(qint64 id, bool active) {
    requireAdmin();
    if (id == session_.user().id)
        throw std::runtime_error("Cannot disable your current login");
    repo_.one("SELECT id FROM users WHERE id=?", {id});
    TransactionGuard tx(repo_.database().connection());
    repo_.execute("UPDATE users SET active=? WHERE id=?", {active ? 1 : 0, id});
    repo_.execute(
        "INSERT INTO audit_logs(actor_id,action,detail) VALUES(?,'User status changed',?)",
        {session_.user().id, QString::number(id)});
    tx.commit();
}
