#pragma once
#include <QSqlDatabase>
class TransactionGuard final {
  public:
    explicit TransactionGuard(QSqlDatabase db);
    ~TransactionGuard();
    TransactionGuard(const TransactionGuard &) = delete;
    TransactionGuard &operator=(const TransactionGuard &) = delete;
    void commit();

  private:
    QSqlDatabase db_;
    bool committed_ = false;
};
