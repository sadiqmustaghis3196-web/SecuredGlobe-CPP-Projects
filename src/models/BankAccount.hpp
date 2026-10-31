#pragma once
#include <QVariantMap>
class BankAccount final {
  public:
    qint64 id = 0, customerId = 0, balance = 0;
    QString number, type, status, nickname;
    static BankAccount fromRecord(const QVariantMap &record);
    void requireActive() const;
};
