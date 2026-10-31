#pragma once
#include "repositories/BankRepository.hpp"
#include <QDate>
class LoanCalculator final {
  public:
    static qint64 payment(qint64 principal, int months, int annualRateBps);
    static TableData schedule(qint64 principal, int months, int annualRateBps, const QDate &start,
                              qint64 paid = 0);
    static qint64 total(qint64 principal, int months, int annualRateBps);
};
