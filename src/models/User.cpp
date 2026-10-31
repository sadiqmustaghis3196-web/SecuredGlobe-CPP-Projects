#include "User.hpp"
User User::fromRecord(const QVariantMap &r) {
    User u;
    u.id = r.value("id").toLongLong();
    u.customerId = r.value("customer_id").toLongLong();
    u.username = r.value("username").toString();
    u.name = r.value("name").toString();
    u.role = r.value("role").toString();
    u.mustChange = r.value("must_change").toBool();
    return u;
}
bool User::can(const QString &permission) const {
    return permissions.contains(permission);
}
