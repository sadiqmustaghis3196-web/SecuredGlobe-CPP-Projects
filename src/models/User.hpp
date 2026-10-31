#pragma once
#include <QString>
#include <QStringList>
#include <QVariantMap>
class User final {
  public:
    qint64 id = 0, customerId = 0;
    QString username, name, role;
    bool mustChange = false;
    QStringList permissions;
    static User fromRecord(const QVariantMap &record);
    bool can(const QString &permission) const;
};
