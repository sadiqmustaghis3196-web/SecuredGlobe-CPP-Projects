#include "MoneyTableItem.hpp"
#include "utils/Money.hpp"
MoneyTableItem::MoneyTableItem(qint64 cents) : QTableWidgetItem(Money::format(cents)) {
    setData(Qt::UserRole + 1, cents);
}
bool MoneyTableItem::operator<(const QTableWidgetItem &other) const {
    return data(Qt::UserRole + 1).toLongLong() < other.data(Qt::UserRole + 1).toLongLong();
}
