#pragma once
#include "core/SessionManager.hpp"
#include "models/BankAccount.hpp"
#include "repositories/BankRepository.hpp"
#include <QDate>
class BankingService final {
  public:
    BankingService(BankRepository &repository, SessionManager &session);
    TableData list(const QString &entity) const;
    TableData transactions(const QDate &from, const QDate &to, const QString &kind = {},
                           qint64 minimum = 0, qint64 maximum = 9223372036854775807LL) const;
    TableData customerHistory(qint64 customerId) const;
    TableData schedule(qint64 loanId) const;
    Record customer(qint64 id) const;
    BankAccount account(qint64 id) const;
    Record loan(qint64 id) const;
    Record transaction(qint64 id) const;
    Record profile() const;
    Record employee(qint64 id) const;
    Record branch(qint64 id) const;
    Record dashboard() const;
    TableData monthlyActivity() const;
    TableData report(const QString &type, const QDate &from, const QDate &to) const;
    QVector<QPair<qint64, QString>> options(const QString &type) const;
    qint64 saveCustomer(qint64 id, const QString &name, const QString &email, const QString &phone,
                        const QString &address, qint64 branch);
    void verifyCustomer(qint64 id, const QString &status, const QString &documentReference);
    qint64 openAccount(qint64 customerId, qint64 typeId, const QString &nickname);
    void editAccount(qint64 accountId, const QString &nickname, qint64 typeId);
    void setAccountStatus(qint64 id, const QString &status);
    qint64 deposit(qint64 accountId, qint64 amount, const QString &note);
    qint64 withdraw(qint64 accountId, qint64 amount, const QString &note);
    qint64 transfer(qint64 from, qint64 to, qint64 amount, const QString &key, const QString &note);
    qint64 applyLoan(qint64 accountId, qint64 principal, int months, int rateBps,
                     const QString &purpose);
    void reviewLoan(qint64 id, bool approve, const QString &reason);
    void repayLoan(qint64 id, qint64 amount);
    qint64 issueCard(qint64 accountId);
    void blockCard(qint64 cardId, bool blocked);
    void saveEmployee(qint64 id, qint64 userId, qint64 branch, const QString &title);
    void saveBranch(qint64 id, const QString &name, const QString &city);
    void setLimit(qint64 cents);
    qint64 limit() const;
    void backup(const QString &destination);
    void resolveNotification(qint64 id);
    void require(const QString &permission) const;
    static qint64 monthlyPayment(qint64 principal, int months, int rateBps);

  private:
    BankRepository &repo_;
    SessionManager &session_;
    void own(qint64 customerId) const;
    void audit(const QString &action, const QString &detail) const;
    qint64 post(qint64 accountId, qint64 amount, const QString &kind, const QString &note) const;
    qint64 cash(qint64 accountId, qint64 amount, const QString &note, bool credit);
};
