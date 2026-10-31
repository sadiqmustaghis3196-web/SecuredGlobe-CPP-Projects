#include "LoanCalculator.hpp"
#include "utils/Money.hpp"
#include <cmath>
#include <limits>
#include <stdexcept>
qint64 LoanCalculator::payment(qint64 p, int n, int bps) {
    Money::positive(p);
    if (n < 1 || n > 360 || bps < 0 || bps > 10000)
        throw std::runtime_error("Invalid loan term or annual rate");
    long double r = static_cast<long double>(bps) / 120000.0L;
    long double result = bps == 0 ? std::ceil(static_cast<long double>(p) / n)
                                  : std::ceil(p * r / (1 - std::pow(1 + r, -n)));
    if (result > static_cast<long double>(std::numeric_limits<qint64>::max()) / n)
        throw std::runtime_error("Loan is too large");
    return static_cast<qint64>(result);
}
TableData LoanCalculator::schedule(qint64 principal, int months, int bps, const QDate &start,
                                   qint64 paid) {
    auto monthly = payment(principal, months, bps);
    TableData out;
    out.headers = {"Installment",         "Due date",         "Payment",
                   "Principal portion",   "Interest portion", "Remaining principal",
                   "Paid toward schedule"};
    for (int i = 1; i <= months; ++i) {
        qint64 interest = static_cast<qint64>(
            std::llround(static_cast<long double>(principal) * bps / 120000.0L));
        qint64 principalPart =
            i == months ? principal : qMin(principal, qMax<qint64>(0, monthly - interest));
        qint64 installment = Money::add(principalPart, interest);
        principal -= principalPart;
        out.rows << QVariantList{i,
                                 start.addMonths(i).toString(Qt::ISODate),
                                 installment,
                                 principalPart,
                                 interest,
                                 principal,
                                 qMin(paid, installment)};
        paid = qMax<qint64>(0, paid - installment);
    }
    return out;
}
qint64 LoanCalculator::total(qint64 principal, int months, int bps) {
    qint64 sum = 0;
    for (const auto &row : schedule(principal, months, bps, QDate(2026, 1, 1)).rows)
        sum = Money::add(sum, row[2].toLongLong());
    return sum;
}
