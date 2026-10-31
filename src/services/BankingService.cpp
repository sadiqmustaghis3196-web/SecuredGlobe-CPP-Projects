#include "BankingService.hpp"
#include "LoanCalculator.hpp"
#include "core/TransactionGuard.hpp"
#include "utils/Money.hpp"
#include <QDate>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QUuid>
#include <cmath>
#include <limits>
#include <stdexcept>
BankingService::BankingService(BankRepository &r, SessionManager &s) : repo_(r), session_(s) {}
void BankingService::require(const QString &p) const {
    const auto &u = session_.user();
    if (u.mustChange)
        throw std::runtime_error("Change your temporary password before using the workspace");
    if (!u.can(p))
        throw std::runtime_error("You do not have permission for this operation");
    auto state = repo_.one("SELECT active,must_change FROM users WHERE id=?", {u.id});
    if (!state["active"].toBool())
        throw std::runtime_error("Your login has been disabled");
    if (state["must_change"].toBool())
        throw std::runtime_error("A password reset requires a new sign-in");
    if (repo_
            .one("SELECT COUNT(*) AS n FROM role_permissions rp JOIN permissions p ON "
                 "p.id=rp.permission_id JOIN user_roles ur ON ur.role_id=rp.role_id WHERE "
                 "ur.user_id=? AND p.name=?",
                 {u.id, p})["n"]
            .toInt() == 0)
        throw std::runtime_error("Your permissions changed; sign in again");
}
void BankingService::own(qint64 id) const {
    const auto &u = session_.user();
    if (u.role == "Customer" && u.customerId != id)
        throw std::runtime_error("You can only access your own records");
}
void BankingService::audit(const QString &a, const QString &d) const {
    repo_.execute("INSERT INTO audit_logs(actor_id,action,detail) VALUES(?,?,?)",
                  {session_.user().id, a, d});
}
Record BankingService::customer(qint64 id) const {
    require("view");
    own(id);
    return repo_.one(
        "SELECT c.*,b.name AS branch,k.status AS kyc,k.document_reference FROM customers c JOIN "
        "branches b ON b.id=c.branch_id JOIN customer_kyc k ON k.customer_id=c.id WHERE c.id=?",
        {id});
}
BankAccount BankingService::account(qint64 id) const {
    require("view");
    auto r = repo_.one("SELECT a.*,t.name AS type FROM bank_accounts a JOIN account_types t ON "
                       "t.id=a.type_id WHERE a.id=?",
                       {id});
    auto a = BankAccount::fromRecord(r);
    own(a.customerId);
    return a;
}
Record BankingService::loan(qint64 id) const {
    require("view");
    auto l = repo_.one("SELECT l.*,c.name AS customer FROM loans l JOIN customers c ON "
                       "c.id=l.customer_id WHERE l.id=?",
                       {id});
    own(l["customer_id"].toLongLong());
    return l;
}
Record BankingService::transaction(qint64 id) const {
    require("view");
    auto t = repo_.one(
        "SELECT t.*,a.number AS account,c.name AS customer,a.customer_id FROM transactions t JOIN "
        "bank_accounts a ON a.id=t.account_id JOIN customers c ON c.id=a.customer_id WHERE t.id=?",
        {id});
    own(t["customer_id"].toLongLong());
    return t;
}
Record BankingService::employee(qint64 id) const {
    require("admin");
    return repo_.one("SELECT * FROM employees WHERE id=?", {id});
}
Record BankingService::branch(qint64 id) const {
    require("admin");
    return repo_.one("SELECT * FROM branches WHERE id=?", {id});
}
Record BankingService::profile() const {
    const auto &u = session_.user();
    auto r = repo_.one("SELECT id,username,name,customer_id FROM users WHERE id=?", {u.id});
    r["role"] = u.role;
    return r;
}
TableData BankingService::list(const QString &entity) const {
    require("view");
    const auto &u = session_.user();
    QString where = u.role == "Customer" ? " WHERE c.id=?" : "";
    QVariantList args;
    if (u.role == "Customer")
        args << u.customerId;
    if (entity == "customers")
        return repo_.table(
            "SELECT c.id AS ID,c.name AS Name,c.email AS Email,c.phone AS Phone,b.name AS "
            "Branch,k.status AS KYC,COALESCE((SELECT GROUP_CONCAT(a.number,' · ') FROM "
            "bank_accounts a WHERE a.customer_id=c.id),'') AS Accounts FROM customers c JOIN "
            "branches b ON b.id=c.branch_id JOIN customer_kyc k ON k.customer_id=c.id" +
                where + " ORDER BY c.id DESC",
            args);
    if (entity == "accounts")
        return repo_.table(
            "SELECT a.id AS ID,a.number AS Account,c.name AS Customer,t.name AS Type,a.balance AS "
            "Balance,a.status AS Status,a.nickname AS Nickname FROM bank_accounts a JOIN customers "
            "c ON c.id=a.customer_id JOIN account_types t ON t.id=a.type_id" +
                where + " ORDER BY a.id DESC",
            args);
    if (entity == "loans")
        return repo_.table(
            "SELECT l.id AS ID,c.name AS Customer,l.principal AS Principal,l.term_months AS "
            "Months,l.status AS Status,l.total_due-l.paid AS Outstanding FROM loans l JOIN "
            "customers c ON c.id=l.customer_id" +
                where + " ORDER BY l.id DESC",
            args);
    if (entity == "cards") {
        if (u.role != "Customer")
            require("cards");
        return repo_.table(
            "SELECT d.id AS ID,d.masked_number AS Card,a.number AS Account,c.name AS "
            "Customer,d.status AS Status,d.issued_at AS Issued FROM debit_cards d JOIN "
            "bank_accounts a ON a.id=d.account_id JOIN customers c ON c.id=a.customer_id" +
                where + " ORDER BY d.id DESC",
            args);
    }
    if (entity == "branches") {
        require("reports");
        return repo_.table(
            "SELECT b.id AS ID,b.name AS Branch,b.city AS City,COUNT(c.id) AS Customers FROM "
            "branches b LEFT JOIN customers c ON c.branch_id=b.id GROUP BY b.id");
    }
    if (entity == "employees") {
        require("admin");
        return repo_.table("SELECT e.id AS ID,u.name AS Name,u.username AS Username,b.name AS "
                           "Branch,e.title AS Title FROM employees e JOIN users u ON "
                           "u.id=e.user_id JOIN branches b ON b.id=e.branch_id");
    }
    if (entity == "users") {
        require("admin");
        return repo_.table(
            "SELECT u.id AS ID,u.username AS Username,u.name AS Name,r.name AS Role,u.active AS "
            "Active,u.must_change AS MustChange,u.failed_attempts AS FailedAttempts FROM users u "
            "JOIN user_roles ur ON ur.user_id=u.id JOIN roles r ON r.id=ur.role_id");
    }
    if (entity == "audit") {
        require("audit");
        return repo_.table("SELECT a.id AS ID,COALESCE(u.username,'System') AS Actor,a.action AS "
                           "Action,a.detail AS Detail,a.created_at AS Date FROM audit_logs a LEFT "
                           "JOIN users u ON u.id=a.actor_id ORDER BY a.id DESC");
    }
    if (entity == "notifications") {
        require("admin");
        return repo_.table(
            "SELECT id AS ID,message AS Message,created_at AS Date FROM notifications WHERE "
            "resolved=0 AND (user_id IS NULL OR user_id=?) ORDER BY id DESC",
            {u.id});
    }
    throw std::runtime_error("Unknown record type");
}
TableData BankingService::transactions(const QDate &from, const QDate &to, const QString &kind,
                                       qint64 minimum, qint64 maximum) const {
    require("view");
    if (!from.isValid() || !to.isValid() || from > to || minimum < 0 || minimum > maximum)
        throw std::runtime_error("Invalid transaction filter");
    QString sql =
        "SELECT t.id AS ID,t.reference AS Reference,a.number AS Account,c.name AS Customer,t.kind "
        "AS Type,t.amount AS Amount,t.created_at AS Date FROM transactions t JOIN bank_accounts a "
        "ON a.id=t.account_id JOIN customers c ON c.id=a.customer_id WHERE date(t.created_at) "
        "BETWEEN ? AND ? AND t.amount BETWEEN ? AND ?";
    QVariantList args{from.toString(Qt::ISODate), to.toString(Qt::ISODate), minimum, maximum};
    if (!kind.isEmpty()) {
        sql += " AND t.kind=?";
        args << kind;
    }
    if (session_.user().role == "Customer") {
        sql += " AND a.customer_id=?";
        args << session_.user().customerId;
    }
    return repo_.table(sql + " ORDER BY t.id DESC", args);
}
TableData BankingService::customerHistory(qint64 id) const {
    require("view");
    own(id);
    return repo_.table("SELECT t.id AS ID,a.number AS Account,t.kind AS Type,t.amount AS "
                       "Amount,t.created_at AS Date FROM transactions t JOIN bank_accounts a ON "
                       "a.id=t.account_id WHERE a.customer_id=? ORDER BY t.id DESC",
                       {id});
}
QVector<QPair<qint64, QString>> BankingService::options(const QString &type) const {
    require("view");
    QString sql;
    QVariantList args;
    if (type == "account_types")
        sql = "SELECT id,name AS label FROM account_types";
    else if (type == "branches") {
        require("customers");
        sql = "SELECT id,name AS label FROM branches WHERE active=1";
    } else if (type == "customers") {
        if (session_.user().role != "Customer")
            require("customers");
        sql = "SELECT id,name AS label FROM customers";
        if (session_.user().role == "Customer") {
            sql += " WHERE id=?";
            args << session_.user().customerId;
        }
    } else if (type == "accounts") {
        sql = "SELECT id,number||' · '||nickname AS label FROM bank_accounts WHERE status='Active'";
        if (session_.user().role == "Customer") {
            sql += " AND customer_id=?";
            args << session_.user().customerId;
        }
    } else if (type == "staff") {
        require("admin");
        sql = "SELECT DISTINCT u.id,u.username||' · '||u.name AS label FROM users u JOIN "
              "user_roles ur ON ur.user_id=u.id JOIN roles r ON r.id=ur.role_id WHERE "
              "r.name<>'Customer' AND u.active=1";
    } else
        throw std::runtime_error("Unknown option list");
    QVector<QPair<qint64, QString>> result;
    for (const auto &r : repo_.records(sql, args))
        result.append({r["id"].toLongLong(), r["label"].toString()});
    return result;
}
qint64 BankingService::saveCustomer(qint64 id, const QString &name, const QString &email,
                                    const QString &phone, const QString &address, qint64 branch) {
    require("customers");
    if (name.trimmed().size() < 2 || name.size() > 100 ||
        !QRegularExpression("^[^\\s@]+@[^\\s@]+\\.[^\\s@]+$").match(email).hasMatch() ||
        phone.trimmed().size() < 5 || address.trimmed().size() < 3)
        throw std::runtime_error("Enter a name, valid email, phone, and address");
    repo_.one("SELECT id FROM branches WHERE id=? AND active=1", {branch});
    TransactionGuard tx(repo_.database().connection());
    if (id) {
        customer(id);
        repo_.execute(
            "UPDATE customers SET name=?,email=?,phone=?,address=?,branch_id=? WHERE id=?",
            {name.trimmed(), email.trimmed().toLower(), phone.trimmed(), address.trimmed(), branch,
             id});
    } else {
        id = repo_.execute(
            "INSERT INTO customers(name,email,phone,address,branch_id) VALUES(?,?,?,?,?)",
            {name.trimmed(), email.trimmed().toLower(), phone.trimmed(), address.trimmed(),
             branch});
        repo_.execute("INSERT INTO customer_kyc(customer_id) VALUES(?)", {id});
    }
    audit("Customer saved", QString::number(id));
    tx.commit();
    return id;
}
void BankingService::verifyCustomer(qint64 id, const QString &status, const QString &reference) {
    require("approve");
    customer(id);
    if (!QStringList{"Verified", "Rejected", "Pending"}.contains(status) ||
        reference.trimmed().isEmpty())
        throw std::runtime_error("Choose a verification outcome and document reference");
    TransactionGuard tx(repo_.database().connection());
    repo_.execute("UPDATE customer_kyc SET "
                  "status=?,document_reference=?,reviewed_by=?,reviewed_at=CURRENT_TIMESTAMP WHERE "
                  "customer_id=?",
                  {status, reference.trimmed(), session_.user().id, id});
    audit("KYC reviewed", QString::number(id) + " " + status);
    tx.commit();
}
qint64 BankingService::openAccount(qint64 customerId, qint64 typeId, const QString &nickname) {
    require("accounts");
    auto c = customer(customerId);
    if (c["kyc"].toString() != "Verified")
        throw std::runtime_error("Customer KYC must be verified before opening an account");
    repo_.one("SELECT id FROM account_types WHERE id=?", {typeId});
    if (nickname.trimmed().isEmpty())
        throw std::runtime_error("Enter an account nickname");
    TransactionGuard tx(repo_.database().connection());
    QString number =
        "SG" + QUuid::createUuid().toString(QUuid::WithoutBraces).remove('-').left(16).toUpper();
    auto id = repo_.execute(
        "INSERT INTO bank_accounts(number,customer_id,type_id,nickname) VALUES(?,?,?,?)",
        {number, customerId, typeId, nickname.trimmed()});
    audit("Account opened", number);
    tx.commit();
    return id;
}
void BankingService::editAccount(qint64 id, const QString &nickname, qint64 typeId) {
    require("accounts");
    auto a = account(id);
    if (a.status == "Closed")
        throw std::runtime_error("Closed accounts cannot be edited");
    if (nickname.trimmed().isEmpty())
        throw std::runtime_error("Enter an account nickname");
    repo_.one("SELECT id FROM account_types WHERE id=?", {typeId});
    TransactionGuard tx(repo_.database().connection());
    repo_.execute("UPDATE bank_accounts SET nickname=?,type_id=? WHERE id=?",
                  {nickname.trimmed(), typeId, id});
    audit("Account edited", a.number);
    tx.commit();
}
void BankingService::setAccountStatus(qint64 id, const QString &status) {
    require("approve");
    auto a = account(id);
    if (a.status == "Closed")
        throw std::runtime_error("Closed accounts cannot be reopened");
    if (!QStringList{"Active", "Frozen", "Closed"}.contains(status) || status == a.status)
        throw std::runtime_error("Invalid account status transition");
    if (status == "Closed" &&
        (a.balance != 0 || repo_.one("SELECT COUNT(*) AS n FROM loans WHERE account_id=? AND "
                                     "status IN('Pending','Approved')",
                                     {id})["n"]
                                   .toInt() > 0))
        throw std::runtime_error(
            "Account closure requires zero balance and no outstanding obligations");
    TransactionGuard tx(repo_.database().connection());
    repo_.execute("UPDATE bank_accounts SET status=? WHERE id=?", {status, id});
    if (status != "Active")
        repo_.execute("UPDATE debit_cards SET status='Blocked' WHERE account_id=?", {id});
    audit("Account status changed", a.number + " / " + status);
    tx.commit();
}
qint64 BankingService::post(qint64 id, qint64 amount, const QString &kind,
                            const QString &note) const {
    return repo_.execute("INSERT INTO transactions(reference,account_id,kind,amount,note,actor_id) "
                         "VALUES(?,?,?,?,?,?)",
                         {"SG-" + QUuid::createUuid().toString(QUuid::WithoutBraces), id, kind,
                          amount, note.isNull() ? QString("") : note, session_.user().id});
}
qint64 BankingService::limit() const {
    require("view");
    return repo_
        .one("SELECT value FROM application_settings WHERE key='transaction_limit'")["value"]
        .toLongLong();
}
qint64 BankingService::cash(qint64 id, qint64 amount, const QString &note, bool credit) {
    require("cash");
    Money::positive(amount);
    TransactionGuard tx(repo_.database().connection());
    auto a = account(id);
    a.requireActive();
    if (!credit && amount > limit())
        throw std::runtime_error("Withdrawal exceeds the configured limit");
    if (!credit && amount > a.balance)
        throw std::runtime_error("Insufficient funds");
    qint64 balance = credit ? Money::add(a.balance, amount) : a.balance - amount;
    repo_.execute("UPDATE bank_accounts SET balance=? WHERE id=?", {balance, id});
    auto tid = post(id, amount, credit ? "Deposit" : "Withdrawal", note);
    audit(credit ? "Cash deposit" : "Cash withdrawal", a.number + " / " + Money::format(amount));
    tx.commit();
    return tid;
}
qint64 BankingService::deposit(qint64 id, qint64 amount, const QString &note) {
    return cash(id, amount, note, true);
}
qint64 BankingService::withdraw(qint64 id, qint64 amount, const QString &note) {
    return cash(id, amount, note, false);
}
qint64 BankingService::transfer(qint64 from, qint64 to, qint64 amount, const QString &key,
                                const QString &note) {
    require("transfer");
    Money::positive(amount);
    if (from == to || key.trimmed().isEmpty() || key.size() > 128)
        throw std::runtime_error("Choose different accounts and a valid idempotency key");
    if (amount > limit())
        throw std::runtime_error("Transfer exceeds the configured limit");
    TransactionGuard tx(repo_.database().connection());
    if (!repo_.records("SELECT id FROM transfers WHERE idempotency_key=?", {key}).isEmpty())
        throw std::runtime_error("Duplicate transfer: this request was already processed");
    auto a = account(from);
    auto target = repo_.one("SELECT a.*,t.name AS type FROM bank_accounts a JOIN account_types t "
                            "ON t.id=a.type_id WHERE a.id=?",
                            {to});
    auto b = BankAccount::fromRecord(target);
    a.requireActive();
    b.requireActive();
    if (amount > a.balance)
        throw std::runtime_error("Insufficient funds");
    repo_.execute("UPDATE bank_accounts SET balance=? WHERE id=?", {a.balance - amount, from});
    repo_.execute("UPDATE bank_accounts SET balance=? WHERE id=?",
                  {Money::add(b.balance, amount), to});
    auto debit = post(from, amount, "Transfer Out", note + " / to " + b.number);
    auto credit = post(to, amount, "Transfer In", note + " / from " + a.number);
    repo_.execute("INSERT INTO "
                  "transfers(idempotency_key,from_account,to_account,amount,debit_transaction,"
                  "credit_transaction) VALUES(?,?,?,?,?,?)",
                  {key, from, to, amount, debit, credit});
    audit("Transfer", a.number + " → " + b.number + " / " + Money::format(amount));
    tx.commit();
    return debit;
}
qint64 BankingService::monthlyPayment(qint64 p, int n, int bps) {
    return LoanCalculator::payment(p, n, bps);
}
qint64 BankingService::applyLoan(qint64 id, qint64 principal, int months, int rate,
                                 const QString &purpose) {
    require("loans");
    auto a = account(id);
    a.requireActive();
    auto c = customer(a.customerId);
    if (c["kyc"].toString() != "Verified")
        throw std::runtime_error("Verified KYC is required");
    if (rate != 850)
        throw std::runtime_error("The demonstration lending rate is fixed at 8.50%");
    if (purpose.trimmed().size() < 3)
        throw std::runtime_error("Enter the loan purpose");
    auto total = LoanCalculator::total(principal, months, rate);
    TransactionGuard tx(repo_.database().connection());
    auto loanId =
        repo_.execute("INSERT INTO "
                      "loans(customer_id,account_id,principal,annual_rate_bps,term_months,purpose,"
                      "total_due) VALUES(?,?,?,?,?,?,?)",
                      {a.customerId, id, principal, rate, months, purpose.trimmed(), total});
    audit("Loan application", QString::number(loanId));
    tx.commit();
    return loanId;
}
void BankingService::reviewLoan(qint64 id, bool approve, const QString &reason) {
    require("approve");
    TransactionGuard tx(repo_.database().connection());
    auto l = loan(id);
    if (l["status"].toString() != "Pending")
        throw std::runtime_error("Only pending applications can be reviewed");
    if (reason.trimmed().size() < 3)
        throw std::runtime_error("Enter a review reason");
    if (approve) {
        auto a = account(l["account_id"].toLongLong());
        a.requireActive();
        if (customer(a.customerId)["kyc"].toString() != "Verified")
            throw std::runtime_error("Customer KYC must be verified");
        auto p = l["principal"].toLongLong();
        repo_.execute("UPDATE bank_accounts SET balance=? WHERE id=?",
                      {Money::add(a.balance, p), a.id});
        post(a.id, p, "Loan Disbursement", QString("Loan %1").arg(id));
    }
    auto total = LoanCalculator::total(l["principal"].toLongLong(), l["term_months"].toInt(),
                                       l["annual_rate_bps"].toInt());
    repo_.execute(
        "UPDATE loans SET status=?,reviewed_by=?,approved_at=CASE WHEN ?=1 THEN CURRENT_TIMESTAMP "
        "ELSE NULL END,total_due=? WHERE id=?",
        {approve ? "Approved" : "Rejected", session_.user().id, approve ? 1 : 0, total, id});
    audit(approve ? "Loan approved" : "Loan rejected", QString::number(id) + " / " + reason);
    tx.commit();
}
void BankingService::repayLoan(qint64 id, qint64 amount) {
    require("loans");
    Money::positive(amount);
    TransactionGuard tx(repo_.database().connection());
    auto l = loan(id);
    if (l["status"].toString() != "Approved")
        throw std::runtime_error("Loan must be approved and outstanding");
    auto remaining = l["total_due"].toLongLong() - l["paid"].toLongLong();
    if (amount > remaining)
        throw std::runtime_error("Payment exceeds outstanding balance");
    auto a = account(l["account_id"].toLongLong());
    a.requireActive();
    if (amount > a.balance)
        throw std::runtime_error("Insufficient funds for loan repayment");
    repo_.execute("UPDATE bank_accounts SET balance=? WHERE id=?", {a.balance - amount, a.id});
    auto tid = post(a.id, amount, "Loan Repayment", QString("Loan %1").arg(id));
    repo_.execute("INSERT INTO loan_repayments(loan_id,transaction_id,amount) VALUES(?,?,?)",
                  {id, tid, amount});
    repo_.execute("UPDATE loans SET paid=paid+?,status=CASE WHEN paid+?=total_due THEN 'Repaid' "
                  "ELSE 'Approved' END WHERE id=?",
                  {amount, amount, id});
    audit("Loan repayment", QString::number(id) + " / " + Money::format(amount));
    tx.commit();
}
TableData BankingService::schedule(qint64 id) const {
    auto l = loan(id);
    auto start = QDate::fromString(l["approved_at"].toString().left(10), Qt::ISODate);
    if (!start.isValid())
        start = QDate::currentDate();
    return LoanCalculator::schedule(l["principal"].toLongLong(), l["term_months"].toInt(),
                                    l["annual_rate_bps"].toInt(), start, l["paid"].toLongLong());
}
qint64 BankingService::issueCard(qint64 id) {
    require("cards");
    auto a = account(id);
    a.requireActive();
    TransactionGuard tx(repo_.database().connection());
    auto masked = "DEMO-" + QUuid::createUuid().toString(QUuid::WithoutBraces).left(8) + "-****";
    auto card = repo_.execute("INSERT INTO debit_cards(account_id,masked_number) VALUES(?,?)",
                              {id, masked});
    audit("Card issued", masked);
    tx.commit();
    return card;
}
void BankingService::blockCard(qint64 id, bool blocked) {
    require("cards");
    auto c = repo_.one("SELECT account_id,status FROM debit_cards WHERE id=?", {id});
    auto a = account(c["account_id"].toLongLong());
    if (!blocked)
        a.requireActive();
    TransactionGuard tx(repo_.database().connection());
    repo_.execute("UPDATE debit_cards SET status=? WHERE id=?",
                  {blocked ? "Blocked" : "Active", id});
    audit(blocked ? "Card blocked" : "Card unblocked", QString::number(id));
    tx.commit();
}
void BankingService::saveEmployee(qint64 id, qint64 user, qint64 branch, const QString &title) {
    require("admin");
    if (id)
        employee(id);
    if (title.trimmed().size() < 2)
        throw std::runtime_error("Enter an employee title");
    repo_.one("SELECT u.id FROM users u JOIN user_roles ur ON ur.user_id=u.id JOIN roles r ON "
              "r.id=ur.role_id WHERE u.id=? AND r.name<>'Customer'",
              {user});
    repo_.one("SELECT id FROM branches WHERE id=?", {branch});
    TransactionGuard tx(repo_.database().connection());
    if (id)
        repo_.execute("UPDATE employees SET user_id=?,branch_id=?,title=? WHERE id=?",
                      {user, branch, title, id});
    else
        repo_.execute("INSERT INTO employees(user_id,branch_id,title) VALUES(?,?,?)",
                      {user, branch, title});
    audit("Employee saved", QString::number(user));
    tx.commit();
}
void BankingService::saveBranch(qint64 id, const QString &name, const QString &city) {
    require("admin");
    if (id)
        branch(id);
    if (name.trimmed().size() < 2 || city.trimmed().size() < 2)
        throw std::runtime_error("Enter a branch name and city");
    TransactionGuard tx(repo_.database().connection());
    if (id)
        repo_.execute("UPDATE branches SET name=?,city=? WHERE id=?",
                      {name.trimmed(), city.trimmed(), id});
    else
        repo_.execute("INSERT INTO branches(name,city) VALUES(?,?)",
                      {name.trimmed(), city.trimmed()});
    audit("Branch saved", name);
    tx.commit();
}
void BankingService::setLimit(qint64 cents) {
    require("admin");
    Money::positive(cents);
    TransactionGuard tx(repo_.database().connection());
    repo_.execute("UPDATE application_settings SET value=? WHERE key='transaction_limit'",
                  {QString::number(cents)});
    audit("Transaction limit changed", Money::format(cents));
    tx.commit();
}
void BankingService::backup(const QString &destination) {
    require("admin");
    if (destination.isEmpty())
        throw std::runtime_error("Choose a backup path");
    if (QFileInfo::exists(destination) || QFileInfo(destination).absoluteFilePath() ==
                                              QFileInfo(repo_.database().path()).absoluteFilePath())
        throw std::runtime_error("Choose a new file name; existing files are never overwritten");
    repo_.execute("VACUUM INTO ?", {destination});
    audit("Database backup", QFileInfo(destination).fileName());
}
void BankingService::resolveNotification(qint64 id) {
    require("admin");
    TransactionGuard tx(repo_.database().connection());
    repo_.execute("UPDATE notifications SET resolved=1 WHERE id=?", {id});
    audit("Notification resolved", QString::number(id));
    tx.commit();
}
Record BankingService::dashboard() const {
    require("view");
    QString suffix;
    QVariantList args;
    if (session_.user().role == "Customer") {
        suffix = " WHERE customer_id=?";
        args << session_.user().customerId;
    }
    Record d = repo_.one("SELECT COALESCE(SUM(balance),0) AS balance,COUNT(*) AS "
                         "accounts,COALESCE(SUM(status='Active'),0) AS active FROM bank_accounts" +
                             suffix,
                         args);
    d["customers"] = session_.user().role == "Customer"
                         ? 1
                         : repo_.one("SELECT COUNT(*) AS n FROM customers")["n"];
    d["pending"] = repo_.one("SELECT COUNT(*) AS n FROM loans" +
                                 (suffix.isEmpty() ? QString(" WHERE status='Pending'")
                                                   : suffix + " AND status='Pending'"),
                             args)["n"];
    auto ts = transactions(QDate(2000, 1, 1), QDate::currentDate());
    qint64 dep = 0, wd = 0;
    for (const auto &t : ts.rows) {
        if (t[4].toString() == "Deposit")
            dep += t[5].toLongLong();
        if (t[4].toString() == "Withdrawal")
            wd += t[5].toLongLong();
    }
    for (const auto &status : QStringList{"Approved", "Rejected", "Repaid"})
        d[status] = repo_.one(
            "SELECT COUNT(*) AS n FROM loans" +
                (suffix.isEmpty() ? QString(" WHERE status=?") : suffix + " AND status=?"),
            suffix.isEmpty() ? QVariantList{status}
                             : QVariantList{session_.user().customerId, status})["n"];
    d["deposits"] = dep;
    d["withdrawals"] = wd;
    return d;
}
TableData BankingService::monthlyActivity() const {
    require("view");
    QString sql =
        "SELECT strftime('%Y-%m',t.created_at) AS Month,SUM(CASE WHEN t.kind "
        "IN('Deposit','Transfer In','Loan Disbursement') THEN t.amount ELSE 0 END) AS "
        "Inflow,SUM(CASE WHEN t.kind IN('Withdrawal','Transfer Out','Loan Repayment') THEN "
        "t.amount ELSE 0 END) AS Outflow FROM transactions t JOIN bank_accounts a ON "
        "a.id=t.account_id WHERE t.created_at>=date('now','-11 months','start of month')";
    QVariantList args;
    if (session_.user().role == "Customer") {
        sql += " AND a.customer_id=?";
        args << session_.user().customerId;
    }
    return repo_.table(sql + " GROUP BY strftime('%Y-%m',t.created_at) ORDER BY Month", args);
}
TableData BankingService::report(const QString &type, const QDate &from, const QDate &to) const {
    require("reports");
    if (!from.isValid() || !to.isValid() || from > to)
        throw std::runtime_error("Invalid report date range");
    if (type == "transactions")
        return transactions(from, to);
    if (type == "customers")
        return list("customers");
    if (type == "accounts")
        return list("accounts");
    if (type == "financial")
        return repo_.table(
            "SELECT t.kind AS Type,COUNT(*) AS Transactions,SUM(t.amount) AS Amount FROM "
            "transactions t WHERE date(t.created_at) BETWEEN ? AND ? GROUP BY t.kind",
            {from.toString(Qt::ISODate), to.toString(Qt::ISODate)});
    if (type == "branches")
        return list("branches");
    throw std::runtime_error("Unknown report");
}
