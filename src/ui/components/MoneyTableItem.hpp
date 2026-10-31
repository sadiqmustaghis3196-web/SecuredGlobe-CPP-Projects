#pragma once
#include <QTableWidgetItem>
class MoneyTableItem final : public QTableWidgetItem {
  public:
    explicit MoneyTableItem(qint64 cents);
    bool operator<(const QTableWidgetItem &other) const override;
};
