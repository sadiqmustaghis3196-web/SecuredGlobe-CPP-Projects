#include "core/Database.hpp"
#include "core/SessionManager.hpp"
#include "repositories/BankRepository.hpp"
#include "services/AuthService.hpp"
#include "services/BankingService.hpp"
#include "services/SeedService.hpp"
#include "ui/MainWindow.hpp"
#include <QAbstractButton>
#include <QComboBox>
#include <QFile>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTextBrowser>
#include <QTimer>
#include <QUuid>
#include <QtTest>
#include <memory>
class GuiTests final : public QObject {
    Q_OBJECT
    QTemporaryDir directory_;
    std::unique_ptr<Database> database_;
    std::unique_ptr<BankRepository> repository_;
    std::unique_ptr<SessionManager> session_;
    std::unique_ptr<AuthService> auth_;
    std::unique_ptr<BankingService> bank_;
    std::unique_ptr<MainWindow> window_;
    QLineEdit *edit(const QString &name) {
        for (auto e : window_->findChildren<QLineEdit *>())
            if (e->accessibleName() == name)
                return e;
        throw std::runtime_error(("Missing input " + name).toStdString());
    }
    void click(const QString &text) {
        QPushButton *found = nullptr;
        for (auto b : window_->findChildren<QPushButton *>())
            if (b->text() == text) {
                found = b;
                break;
            }
        if (!found)
            throw std::runtime_error(("Missing button " + text).toStdString());
        found->click();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        qApp->processEvents();
    }
    void screen(int id) {
        window_->navigate(id);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        qApp->processEvents();
    }
    void confirm() {
        QTimer::singleShot(0, [] {
            if (auto box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget()))
                box->button(QMessageBox::Yes)->click();
        });
    }
  private slots:
    void initTestCase() {
        Database db(directory_.filePath("seed.sqlite"));
        BankRepository r(db);
        SeedService::initialize(r);
        SessionManager s;
        AuthService a(r, s);
        a.login("admin", "DemoBank!2026");
        a.changePassword("DemoBank!2026", "GuiTesting!2026");
    }
    void init() {
        auto path =
            directory_.filePath(QUuid::createUuid().toString(QUuid::WithoutBraces) + ".sqlite");
        QVERIFY(QFile::copy(directory_.filePath("seed.sqlite"), path));
        database_ = std::make_unique<Database>(path);
        repository_ = std::make_unique<BankRepository>(*database_);
        session_ = std::make_unique<SessionManager>();
        auth_ = std::make_unique<AuthService>(*repository_, *session_);
        bank_ = std::make_unique<BankingService>(*repository_, *session_);
        auth_->login("admin", "GuiTesting!2026");
        window_ = std::make_unique<MainWindow>(*bank_, *auth_, *session_);
        window_->show();
        qApp->processEvents();
    }
    void cleanup() {
        window_.reset();
        bank_.reset();
        auth_.reset();
        session_.reset();
        repository_.reset();
        database_.reset();
    }
    void depositAndReceipt() {
        auto balance = bank_->account(1).balance;
        screen(20);
        edit("Amount (USD)")->setText("25.00");
        edit("Reference / note")->setText("GUI deposit");
        confirm();
        click("Confirm deposit");
        QCOMPARE(bank_->account(1).balance, balance + 2500);
        auto receipt = window_->findChild<QTextBrowser *>();
        QVERIFY(receipt);
        QVERIFY(receipt->toPlainText().contains("$25.00"));
        QVERIFY(receipt->toPlainText().contains("Completed"));
    }
    void transferAndLedger() {
        auto from = bank_->account(1).balance, to = bank_->account(2).balance;
        screen(22);
        edit("Recipient account ID")->setText("2");
        edit("Amount (USD)")->setText("12.34");
        edit("Reference / note")->setText("GUI transfer");
        confirm();
        click("Confirm transfer");
        QCOMPARE(bank_->account(1).balance, from - 1234);
        QCOMPARE(bank_->account(2).balance, to + 1234);
        QCOMPARE(repository_->one("SELECT COUNT(*) AS n FROM transfers")["n"].toInt(), 1);
    }
    void customerKycAndAccount() {
        screen(8);
        edit("Full name")->setText("GUI Demo Customer");
        edit("Email")->setText("gui@example.com");
        edit("Phone")->setText("123456789");
        edit("Address")->setText("123 Demo Avenue");
        click("Create customer");
        auto c = repository_->one("SELECT id FROM customers WHERE email='gui@example.com'");
        auto cid = c["id"].toLongLong();
        screen(11);
        edit("Document reference")->setText("GUI-DEMO-ID");
        auto combos = window_->findChildren<QComboBox *>();
        QVERIFY(!combos.isEmpty());
        combos.first()->setCurrentText("Verified");
        click("Save KYC decision");
        QCOMPARE(bank_->customer(cid)["kyc"].toString(), QString("Verified"));
        screen(14);
        combos = window_->findChildren<QComboBox *>();
        QVERIFY(combos.size() >= 2);
        combos[0]->setCurrentIndex(combos[0]->findData(cid));
        edit("Account nickname")->setText("GUI savings");
        click("Open new account");
        QCOMPARE(
            repository_
                ->one("SELECT COUNT(*) AS n FROM bank_accounts WHERE customer_id=?", {cid})["n"]
                .toInt(),
            1);
    }
    void loanApprovalAndRepayment() {
        screen(27);
        auto combos = window_->findChildren<QComboBox *>();
        QVERIFY(!combos.isEmpty());
        combos[0]->setCurrentIndex(combos[0]->findData(2));
        edit("Principal (USD)")->setText("1000.00");
        edit("Purpose")->setText("GUI education loan");
        click("Submit application");
        auto row = repository_->one("SELECT id FROM loans ORDER BY id DESC LIMIT 1");
        auto lid = row["id"].toLongLong();
        screen(28);
        edit("Decision reason")->setText("Demo identity verified");
        confirm();
        click("Approve & disburse loan");
        QCOMPARE(bank_->loan(lid)["status"].toString(), QString("Approved"));
        screen(29);
        edit("Repayment amount (USD)")->setText("10.00");
        confirm();
        click("Record repayment");
        QCOMPARE(bank_->loan(lid)["paid"].toLongLong(), 1000LL);
    }
};
QTEST_MAIN(GuiTests)
#include "GuiTests.moc"
