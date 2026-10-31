#include "BankRepository.hpp"
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <stdexcept>
static QSqlQuery prepared(QSqlDatabase db, const QString &sql, const QVariantList &args) {
    QSqlQuery q(db);
    if (!q.prepare(sql))
        throw std::runtime_error(q.lastError().text().toStdString());
    for (const auto &a : args)
        q.addBindValue(a);
    if (!q.exec())
        throw std::runtime_error(q.lastError().text().toStdString());
    return q;
}
BankRepository::BankRepository(Database &database) : database_(database) {}
QVector<Record> BankRepository::records(const QString &sql, const QVariantList &args) const {
    auto q = prepared(database_.connection(), sql, args);
    QVector<Record> result;
    while (q.next()) {
        Record row;
        for (int i = 0; i < q.record().count(); ++i)
            row.insert(q.record().fieldName(i), q.value(i));
        result.append(row);
    }
    return result;
}
Record BankRepository::one(const QString &sql, const QVariantList &args) const {
    auto rows = records(sql, args);
    if (rows.isEmpty())
        throw std::runtime_error("Record not found");
    return rows.first();
}
qint64 BankRepository::execute(const QString &sql, const QVariantList &args) const {
    auto q = prepared(database_.connection(), sql, args);
    return q.lastInsertId().toLongLong();
}
TableData BankRepository::table(const QString &sql, const QVariantList &args) const {
    auto q = prepared(database_.connection(), sql, args);
    TableData out;
    for (int i = 0; i < q.record().count(); ++i)
        out.headers << q.record().fieldName(i);
    while (q.next()) {
        QVariantList row;
        for (int i = 0; i < q.record().count(); ++i)
            row << q.value(i);
        out.rows << row;
    }
    return out;
}
Database &BankRepository::database() const {
    return database_;
}
