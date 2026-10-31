#include "Money.hpp"
#include <QLocale>
#include <QRegularExpression>
#include <limits>
#include <stdexcept>
qint64 Money::parse(const QString &text) {
    QString s = text.trimmed();
    if (!QRegularExpression("^[0-9]{1,14}(\\.[0-9]{1,2})?$").match(s).hasMatch())
        throw std::runtime_error("Enter a positive amount with at most two decimal places");
    auto parts = s.split('.');
    bool ok = false;
    qint64 whole = parts[0].toLongLong(&ok);
    if (!ok)
        throw std::runtime_error("Invalid amount");
    qint64 cents =
        whole * 100 + (parts.size() == 2 ? parts[1].leftJustified(2, '0').toLongLong() : 0);
    positive(cents);
    return cents;
}
QString Money::format(qint64 c) {
    const bool negative = c < 0;
    quint64 v = negative ? static_cast<quint64>(-(c + 1)) + 1 : static_cast<quint64>(c);
    return (negative ? "-$" : "$") +
           QLocale(QLocale::English, QLocale::UnitedStates).toString(v / 100) + "." +
           QString::number(v % 100).rightJustified(2, '0');
}
void Money::positive(qint64 c) {
    if (c <= 0)
        throw std::runtime_error("Amount must be positive");
}
qint64 Money::add(qint64 b, qint64 a) {
    if (a < 0 || b < 0 || b > std::numeric_limits<qint64>::max() - a)
        throw std::runtime_error("Balance would overflow");
    return b + a;
}
