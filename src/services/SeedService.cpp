#include "SeedService.hpp"
#include "LoanCalculator.hpp"
#include "core/TransactionGuard.hpp"
#include "utils/PasswordHasher.hpp"
#include <QDateTime>
#include <QUuid>
void SeedService::initialize(BankRepository &r) {
    if (r.one("SELECT COUNT(*) AS n FROM roles")["n"].toInt() > 0)
        return;
    TransactionGuard tx(r.database().connection());
    const QStringList roles = {"Administrator", "Bank Manager", "Bank Teller", "Customer"};
    const QStringList perms = {"view",    "customers", "accounts", "cash",  "transfer", "loans",
                               "approve", "reports",   "cards",    "admin", "audit"};
    for (const auto &p : perms)
        r.execute("INSERT INTO permissions(name) VALUES(?)", {p});
    for (const auto &role : roles) {
        qint64 id = r.execute("INSERT INTO roles(name) VALUES(?)", {role});
        QStringList allowed;
        if (role == "Administrator")
            allowed = perms;
        else if (role == "Bank Manager")
            allowed = {"view",  "customers", "accounts", "cash", "transfer",
                       "loans", "approve",   "reports",  "cards"};
        else if (role == "Bank Teller")
            allowed = {"view", "customers", "accounts", "cash", "transfer"};
        else
            allowed = {"view", "transfer", "loans"};
        for (const auto &p : allowed)
            r.execute("INSERT INTO role_permissions SELECT ?,id FROM permissions WHERE name=?",
                      {id, p});
    }
    for (const auto &b : QStringList{"Downtown", "Westside", "Riverside"})
        r.execute("INSERT INTO branches(name,city) VALUES(?,?)", {b, "Fictional City"});
    for (const auto &t : QStringList{"Savings", "Current"})
        r.execute("INSERT INTO account_types(name) VALUES(?)", {t});
    r.execute("INSERT INTO application_settings VALUES('transaction_limit','2500000')");
    const QStringList names = {
        "Olivia Rhye",    "Phoenix Baker",   "Lana Steiner", "Demi Wilkinson", "Drew Cano",
        "Natali Craig",   "Orlando Diggs",   "Andi Lane",    "Kate Morrison",  "Ahmed Khan",
        "Sarah Wilson",   "James Miller",    "Ayesha Malik", "Michael Chen",   "Sophia Brown",
        "Hassan Ali",     "Emma Davis",      "William Lee",  "Zara Ahmed",     "Noah Taylor",
        "Isabella Clark", "Ethan Hall",      "Mia Young",    "Lucas King",     "Amelia Scott",
        "Henry Green",    "Charlotte Adams", "Daniel Lewis", "Grace Walker",   "Sadiq Ameen"};
    for (int i = 0; i < names.size(); ++i) {
        auto id =
            r.execute("INSERT INTO customers(name,email,phone,address,branch_id) VALUES(?,?,?,?,?)",
                      {names[i], QString("demo%1@example.com").arg(i + 1),
                       QString("+1 202 555 %1").arg(1000 + i),
                       QString("%1 Example Street").arg(i + 1), i % 3 + 1});
        r.execute("INSERT INTO customer_kyc(customer_id,status) VALUES(?,?)",
                  {id, i % 7 ? "Verified" : "Pending"});
    }
    const QStringList users = {"admin", "manager", "teller", "customer"};
    for (int i = 0; i < 4; ++i) {
        auto salt = PasswordHasher::salt();
        auto id =
            r.execute("INSERT INTO users(username,name,password_hash,salt,iterations,customer_id) "
                      "VALUES(?,?,?,?,?,?)",
                      {users[i], i == 3 ? "Phoenix Baker" : users[i].toUpper(),
                       PasswordHasher::hash("DemoBank!2026", salt), salt,
                       PasswordHasher::Iterations, i == 3 ? QVariant(2) : QVariant()});
        r.execute("INSERT INTO user_roles VALUES(?,?)", {id, i + 1});
        if (i < 3)
            r.execute("INSERT INTO employees(user_id,branch_id,title) VALUES(?,?,?)",
                      {id, i + 1, roles[i]});
    }
    for (int i = 0; i < 40; ++i) {
        auto id = r.execute(
            "INSERT INTO bank_accounts(number,customer_id,type_id,balance,status,nickname) "
            "VALUES(?,?,?,?,?,?)",
            {QString("SG%1").arg(2084000101LL + i), i % 30 + 1, i % 2 + 1, 0,
             i == 8 ? "Frozen" : "Active", i % 2 ? "Everyday" : "Savings"});
        qint64 balance = 0;
        for (int j = 0; j < 3; ++j) {
            qint64 amount = j == 1 ? 5000 + i * 31 : 50000 + i * 1007 + j * 923;
            balance += j == 1 ? -amount : amount;
            r.execute(
                "INSERT INTO "
                "transactions(reference,account_id,kind,amount,note,actor_id,created_at) "
                "VALUES(?,?,?,?,?,1,?)",
                {QUuid::createUuid().toString(QUuid::WithoutBraces), id,
                 j == 1 ? QString("Withdrawal") : QString("Deposit"), amount,
                 j == 1 ? QString("Fictional cash withdrawal") : QString("Fictional cash funding"),
                 QDateTime::currentDateTimeUtc()
                     .addDays(-60 + i + j * 5)
                     .toString("yyyy-MM-dd HH:mm:ss")});
        }
        r.execute("UPDATE bank_accounts SET balance=? WHERE id=?", {balance, id});
        if (i < 10)
            r.execute("INSERT INTO debit_cards(account_id,masked_number,status) VALUES(?,?,?)",
                      {id, QString("DEMO-%1-****-%2").arg(i + 1).arg(4800 + i * 13),
                       i == 8 ? QString("Blocked") : QString("Active")});
    }
    for (int i = 0; i < 8; ++i)
        r.execute(
            "INSERT INTO "
            "loans(customer_id,account_id,principal,annual_rate_bps,term_months,purpose,total_due) "
            "VALUES(?,?,?,850,24,'Fictional education loan',?)",
            {i + 2, i + 2, 100000 + i * 20000, LoanCalculator::total(100000 + i * 20000, 24, 850)});
    r.execute("INSERT INTO audit_logs(actor_id,action,detail) VALUES(1,'Initialized','Fictional "
              "demo dataset created')");
    r.execute("INSERT INTO notifications(message) VALUES('Welcome. All records are fictional "
              "demonstration data.')");
    tx.commit();
}
