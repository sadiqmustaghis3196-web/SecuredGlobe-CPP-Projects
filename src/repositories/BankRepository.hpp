#pragma once
#include "core/Database.hpp"
#include <QMap>
#include <QVariant>
#include <QVector>
using Record = QVariantMap;
struct TableData {
    QStringList headers;
    QVector<QVariantList> rows;
};
class BankRepository final {
  public:
    explicit BankRepository(Database &database);
    QVector<Record> records(const QString &sql, const QVariantList &args = {}) const;
    Record one(const QString &sql, const QVariantList &args = {}) const;
    qint64 execute(const QString &sql, const QVariantList &args = {}) const;
    TableData table(const QString &sql, const QVariantList &args = {}) const;
    Database &database() const;

  private:
    Database &database_;
};
