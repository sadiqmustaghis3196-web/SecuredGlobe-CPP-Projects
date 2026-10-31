#include "core/Database.hpp"
#include "core/SessionManager.hpp"
#include "core/TransactionGuard.hpp"
#include "repositories/BankRepository.hpp"
#include "services/AuthService.hpp"
#include "services/BankingService.hpp"
#include "services/LoanCalculator.hpp"
#include "services/SeedService.hpp"
#include "utils/Money.hpp"
#include "utils/PasswordHasher.hpp"
#include <QDateTime>
#include <QFile>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest>
#include <limits>
#include <memory>
class BankingTests final : public QObject {
    Q_OBJECT
  private:
    QTemporaryDir directory_;
    std::unique_ptr<Database> database_;
    std::unique_ptr<BankRepository> repository_;
    std::unique_ptr<SessionManager> session_;
    std::unique_ptr<AuthService> auth_;
    std::unique_ptr<BankingService> bank_;
    QString dbPath_;
    void admin() {
        auth_->login("admin", "DemoBank!2026");
        auth_->changePassword("DemoBank!2026", "TestingAdmin!2026");
    }
  private slots:
    void initTestCase() {
        Database seed(directory_.filePath("seed.sqlite"));
        BankRepository repo(seed);
        SeedService::initialize(repo);
    }
    void init() {
        dbPath_ =
            directory_.filePath(QUuid::createUuid().toString(QUuid::WithoutBraces) + ".sqlite");
        QVERIFY(QFile::copy(directory_.filePath("seed.sqlite"), dbPath_));
        database_ = std::make_unique<Database>(dbPath_);
        repository_ = std::make_unique<BankRepository>(*database_);
        session_ = std::make_unique<SessionManager>();
        auth_ = std::make_unique<AuthService>(*repository_, *session_);
        bank_ = std::make_unique<BankingService>(*repository_, *session_);
    }
    void cleanup() {
        bank_.reset();
        auth_.reset();
        session_.reset();
        repository_.reset();
        database_.reset();
    }
    void seedAndMoney() {
        QCOMPARE(repository_->one("SELECT COUNT(*) AS n FROM customers")["n"].toInt(), 30);
        QCOMPARE(repository_->one("SELECT COUNT(*) AS n FROM bank_accounts")["n"].toInt(), 40);
        QCOMPARE(repository_->one("SELECT COUNT(*) AS n FROM transactions")["n"].toInt(), 120);
        QCOMPARE(Money::parse("12.03"), 1203LL);
        QCOMPARE(Money::format(1203), QString("$12.03"));
        QVERIFY_EXCEPTION_THROWN(Money::parse("0"), std::runtime_error);
        QVERIFY_EXCEPTION_THROWN(Money::parse("1.001"), std::runtime_error);
        QVERIFY_EXCEPTION_THROWN(Money::parse("-1"), std::runtime_error);
        QVERIFY_EXCEPTION_THROWN(Money::add(std::numeric_limits<qint64>::max(), 1),
                                 std::runtime_error);
    }
    void passwordHashing() {
        auto a = PasswordHasher::salt(), b = PasswordHasher::salt();
        QVERIFY(a != b);
        auto hash = PasswordHasher::hash("StrongPassword!12", a);
        QVERIFY(PasswordHasher::verify("StrongPassword!12", a, hash, PasswordHasher::Iterations));
        QVERIFY(!PasswordHasher::verify("wrong", a, hash, PasswordHasher::Iterations));
        QVERIFY_EXCEPTION_THROWN(PasswordHasher::validate("weak"), std::runtime_error);
    }
    void loginAndPasswordChange() {
        auth_->login("admin", "DemoBank!2026");
        QVERIFY(session_->active());
        QVERIFY_EXCEPTION_THROWN(bank_->dashboard(), std::runtime_error);
        auth_->changePassword("DemoBank!2026", "TestingAdmin!2026");
        QVERIFY(!session_->user().mustChange);
        bank_->dashboard();
        auth_->logout();
        QVERIFY(!session_->active());
        QVERIFY_EXCEPTION_THROWN(auth_->login("admin", "wrong"), std::runtime_error);
        auth_->login("admin", "TestingAdmin!2026");
        QCOMPARE(repository_->one("SELECT failed_attempts FROM users WHERE id=1")["failed_attempts"]
                     .toInt(),
                 0);
    }
    void lockout() {
        for (int i = 0; i < 3; ++i)
            QVERIFY_EXCEPTION_THROWN(auth_->login("teller", "wrong"), std::runtime_error);
        auto row = repository_->one("SELECT * FROM users WHERE username='teller'");
        QCOMPARE(row["failed_attempts"].toInt(), 3);
        QVERIFY(row["locked_until"].toLongLong() >= QDateTime::currentSecsSinceEpoch() + 295);
        QVERIFY_EXCEPTION_THROWN(auth_->login("teller", "DemoBank!2026"), std::runtime_error);
        repository_->execute("UPDATE users SET locked_until=? WHERE username='teller'",
                             {QDateTime::currentSecsSinceEpoch() - 1});
        auth_->login("teller", "DemoBank!2026");
        QVERIFY(session_->active());
    }
    void rbacAndOwnership() {
        auth_->login("customer", "DemoBank!2026");
        auth_->changePassword("DemoBank!2026", "TestingUser!2026");
        QVERIFY_EXCEPTION_THROWN(bank_->deposit(2, 100, "test"), std::runtime_error);
        QVERIFY_EXCEPTION_THROWN(bank_->account(1), std::runtime_error);
        QVERIFY_EXCEPTION_THROWN(bank_->list("audit"), std::runtime_error);
        QVERIFY_EXCEPTION_THROWN(
            auth_->createUser("test", "Test", "Bank Teller", "TestingUser!2026"),
            std::runtime_error);
        auto rows = bank_->list("accounts").rows;
        QCOMPARE(rows.size(), 2);
        auto balance = bank_->account(2).balance;
        bank_->transfer(2, 1, 500, "own-to-external", "allowed recipient");
        QCOMPARE(bank_->account(2).balance, balance - 500);
        QVERIFY_EXCEPTION_THROWN(bank_->transfer(1, 2, 500, "unauthorized", "bad"),
                                 std::runtime_error);
    }
    void customerAndAccount() {
        admin();
        auto id = bank_->saveCustomer(0, "Demo Test", "testing@example.com", "123456789",
                                      "Test Street", 1);
        QVERIFY_EXCEPTION_THROWN(bank_->openAccount(id, 1, "Savings"), std::runtime_error);
        bank_->verifyCustomer(id, "Verified", "DEMO-ID-001");
        auto a = bank_->openAccount(id, 1, "Savings");
        QCOMPARE(bank_->account(a).balance, 0LL);
        bank_->editAccount(a, "Updated", 2);
        QCOMPARE(bank_->account(a).type, QString("Current"));
        QVERIFY_EXCEPTION_THROWN(bank_->saveCustomer(0, "X", "bad", "1", "a", 1),
                                 std::runtime_error);
        QVERIFY_EXCEPTION_THROWN(
            bank_->saveCustomer(0, "Other", "testing@example.com", "123456", "Street", 1),
            std::runtime_error);
    }
    void cashAndFrozen() {
        admin();
        auto before = bank_->account(1).balance;
        auto tid = bank_->deposit(1, 12345, "test deposit");
        QCOMPARE(bank_->account(1).balance, before + 12345);
        QCOMPARE(bank_->transaction(tid)["amount"].toLongLong(), 12345LL);
        bank_->withdraw(1, 345, "test withdrawal");
        QCOMPARE(bank_->account(1).balance, before + 12000);
        QVERIFY_EXCEPTION_THROWN(bank_->withdraw(1, before + 12001, "bad"), std::runtime_error);
        QVERIFY_EXCEPTION_THROWN(bank_->withdraw(1, bank_->limit() + 1, "limit"),
                                 std::runtime_error);
        QVERIFY_EXCEPTION_THROWN(bank_->deposit(1, 0, "bad"), std::runtime_error);
        bank_->setAccountStatus(1, "Frozen");
        QVERIFY_EXCEPTION_THROWN(bank_->deposit(1, 10, "frozen"), std::runtime_error);
        QVERIFY_EXCEPTION_THROWN(bank_->withdraw(1, 10, "frozen"), std::runtime_error);
        bank_->setAccountStatus(1, "Active");
        QCOMPARE(bank_->account(1).status, QString("Active"));
    }
    void transferAtomicAndDuplicate() {
        admin();
        auto a = bank_->account(1).balance, b = bank_->account(2).balance;
        bank_->transfer(1, 2, 5000, "unique-key", "test");
        QCOMPARE(bank_->account(1).balance, a - 5000);
        QCOMPARE(bank_->account(2).balance, b + 5000);
        QVERIFY_EXCEPTION_THROWN(bank_->transfer(1, 2, 5000, "unique-key", "duplicate"),
                                 std::runtime_error);
        QCOMPARE(bank_->account(1).balance, a - 5000);
        QVERIFY_EXCEPTION_THROWN(bank_->transfer(1, 1, 100, "same", "bad"), std::runtime_error);
        QVERIFY_EXCEPTION_THROWN(bank_->transfer(1, 9, 100, "frozen", "bad"), std::runtime_error);
        auto count = repository_->one("SELECT COUNT(*) AS n FROM transactions")["n"].toLongLong();
        repository_->execute(
            "CREATE TRIGGER fail_credit BEFORE INSERT ON transactions WHEN NEW.kind='Transfer In' "
            "BEGIN SELECT RAISE(ABORT,'simulated credit failure'); END");
        QVERIFY_EXCEPTION_THROWN(bank_->transfer(1, 2, 300, "rollback", "test rollback"),
                                 std::runtime_error);
        QCOMPARE(bank_->account(1).balance, a - 5000);
        QCOMPARE(bank_->account(2).balance, b + 5000);
        QCOMPARE(repository_->one("SELECT COUNT(*) AS n FROM transactions")["n"].toLongLong(),
                 count);
        QCOMPARE(
            repository_
                ->one("SELECT COUNT(*) AS n FROM transfers WHERE idempotency_key='rollback'")["n"]
                .toInt(),
            0);
    }
    void loansAndRepayments() {
        admin();
        QCOMPARE(LoanCalculator::total(10001, 12, 0), 10001LL);
        QVERIFY(BankingService::monthlyPayment(100000, 12, 850) > 8333);
        auto before = bank_->account(2).balance;
        auto id = bank_->applyLoan(2, 100000, 12, 850, "Education");
        bank_->reviewLoan(id, true, "Verified application");
        QCOMPARE(bank_->account(2).balance, before + 100000);
        QVERIFY_EXCEPTION_THROWN(bank_->reviewLoan(id, true, "duplicate"), std::runtime_error);
        auto loan = bank_->loan(id);
        auto total = loan["total_due"].toLongLong();
        auto schedule = bank_->schedule(id);
        qint64 sum = 0, principal = 0;
        for (const auto &row : schedule.rows) {
            sum += row[2].toLongLong();
            principal += row[3].toLongLong();
            QCOMPARE(row[2].toLongLong(), row[3].toLongLong() + row[4].toLongLong());
        }
        QCOMPARE(sum, total);
        QCOMPARE(principal, 100000LL);
        QCOMPARE(schedule.rows.last()[5].toLongLong(), 0LL);
        QVERIFY_EXCEPTION_THROWN(bank_->repayLoan(id, total + 1), std::runtime_error);
        bank_->repayLoan(id, 1000);
        QCOMPARE(bank_->loan(id)["paid"].toLongLong(), 1000LL);
        bank_->deposit(2, total, "repayment funding");
        bank_->repayLoan(id, total - 1000);
        QCOMPARE(bank_->loan(id)["status"].toString(), QString("Repaid"));
        QVERIFY_EXCEPTION_THROWN(bank_->repayLoan(id, 1), std::runtime_error);
    }
    void closureCardsAndBackup() {
        admin();
        auto id = bank_->openAccount(2, 1, "Empty");
        auto card = bank_->issueCard(id);
        bank_->blockCard(card, true);
        bank_->blockCard(card, false);
        bank_->setAccountStatus(id, "Closed");
        QVERIFY_EXCEPTION_THROWN(bank_->deposit(id, 1, "closed"), std::runtime_error);
        QVERIFY_EXCEPTION_THROWN(bank_->blockCard(card, false), std::runtime_error);
        QVERIFY_EXCEPTION_THROWN(bank_->setAccountStatus(1, "Closed"), std::runtime_error);
        auto backup = directory_.filePath(
            "backup-" + QUuid::createUuid().toString(QUuid::WithoutBraces) + ".sqlite");
        bank_->backup(backup);
        Database saved(backup);
        BankRepository other(saved);
        QCOMPARE(
            other.one("SELECT status FROM bank_accounts WHERE id=?", {id})["status"].toString(),
            QString("Closed"));
        QVERIFY_EXCEPTION_THROWN(bank_->backup(backup), std::runtime_error);
    }
    void persistenceAuditAndFilters() {
        admin();
        bank_->deposit(1, 4321, "persistent");
        auto balance = bank_->account(1).balance;
        QVERIFY(repository_
                    ->one("SELECT COUNT(*) AS n FROM audit_logs WHERE action='Cash deposit'")["n"]
                    .toInt() > 0);
        auto filtered =
            bank_->transactions(QDateTime::currentDateTimeUtc().date(),
                                QDateTime::currentDateTimeUtc().date(), "Deposit", 4321, 4321);
        QCOMPARE(filtered.rows.size(), 1);
        QVERIFY_EXCEPTION_THROWN(bank_->transactions(QDate::currentDate(), QDate(2000, 1, 1)),
                                 std::runtime_error);
        bank_.reset();
        auth_.reset();
        session_.reset();
        repository_.reset();
        database_.reset();
        database_ = std::make_unique<Database>(dbPath_);
        repository_ = std::make_unique<BankRepository>(*database_);
        QCOMPARE(repository_->one("SELECT balance FROM bank_accounts WHERE id=1")["balance"]
                     .toLongLong(),
                 balance);
    }
    void adminUsersAndReset() {
        admin();
        auto uid = auth_->createUser("newteller", "New Teller", "Bank Teller", "TemporaryUser!12");
        auth_->changeRole(uid, "Bank Manager");
        auth_->resetPassword(uid, "ResetPassword!12");
        auth_->requestReset("newteller");
        QVERIFY(
            repository_
                ->one(
                    "SELECT COUNT(*) AS n FROM notifications WHERE message LIKE '%newteller%'")["n"]
                .toInt() > 0);
        auth_->setActive(uid, false);
        QVERIFY_EXCEPTION_THROWN(auth_->login("newteller", "ResetPassword!12"), std::runtime_error);
        auth_->login("admin", "TestingAdmin!2026");
        auth_->setActive(uid, true);
        auth_->login("newteller", "ResetPassword!12");
        QVERIFY(session_->user().mustChange);
        QCOMPARE(session_->user().role, QString("Bank Manager"));
    }
};
QTEST_GUILESS_MAIN(BankingTests)
#include "BankingTests.moc"
