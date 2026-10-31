#include "BankAccount.hpp"
#include <stdexcept>
BankAccount BankAccount::fromRecord(const QVariantMap &r) {
    BankAccount a;
    a.id = r.value("id").toLongLong();
    a.customerId = r.value("customer_id").toLongLong();
    a.balance = r.value("balance").toLongLong();
    a.number = r.value("number").toString();
    a.type = r.value("type").toString();
    a.status = r.value("status").toString();
    a.nickname = r.value("nickname").toString();
    return a;
}
void BankAccount::requireActive() const {
    if (status != "Active")
        throw std::runtime_error("Account must be active for this operation");
}
