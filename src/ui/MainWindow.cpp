#include "MainWindow.hpp"
#include "components/ActivityChart.hpp"
#include "components/FormDialog.hpp"
#include "components/RecordTable.hpp"
#include "utils/Money.hpp"
#include <QApplication>
#include <QComboBox>
#include <QDate>
#include <QDateEdit>
#include <QDebug>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPrintDialog>
#include <QPrinter>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QSplitter>
#include <QTextBrowser>
#include <QTextDocument>
#include <QTimer>
#include <QUuid>
#include <QVBoxLayout>
#include <limits>
#include <memory>
#include <stdexcept>
static QLabel *textLabel(const QString &text) {
    auto label = new QLabel(text);
    label->setWordWrap(true);
    label->setTextFormat(Qt::PlainText);
    return label;
}
static QComboBox *optionBox(const QVector<QPair<qint64, QString>> &options,
                            QWidget *parent = nullptr) {
    auto box = new QComboBox(parent);
    for (const auto &o : options)
        box->addItem(o.second, o.first);
    return box;
}
static QLineEdit *input(QFormLayout *f, const QString &label, const QString &value = {}) {
    auto w = new QLineEdit(value);
    w->setAccessibleName(label);
    f->addRow(label, w);
    return w;
}
static QVBoxLayout *vertical(QWidget *p) {
    auto l = new QVBoxLayout(p);
    l->setSpacing(16);
    return l;
}
MainWindow::MainWindow(BankingService &b, AuthService &a, SessionManager &s)
    : bank_(b), auth_(a), session_(s) {
    setWindowTitle("SecuredGlobe · Advanced Banking Management");
    resize(1320, 900);
    setMinimumSize(960, 680);
    auto root = new QWidget;
    auto row = new QHBoxLayout(root);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(0);
    auto sidebar = new QWidget;
    sidebar->setObjectName("sidebar");
    sidebar->setFixedWidth(225);
    auto nav = new QVBoxLayout(sidebar);
    nav->setContentsMargins(18, 28, 18, 20);
    auto brand = new QLabel("<span style='font-size:19px'>◈ SecuredGlobe</span><br><span "
                            "style='font-size:9px'>BANKING MANAGEMENT</span>");
    brand->setObjectName("brand");
    nav->addWidget(brand);
    nav->addSpacing(20);
    const QVector<QPair<int, QString>> links = {
        {4, "▦  Dashboard"},     {7, "♧  Customers"},        {13, "▣  Bank Accounts"},
        {19, "⇄  Transactions"}, {22, "↗  Transfers"},       {20, "↓  Deposits"},
        {21, "↑  Withdrawals"},  {26, "▤  Loans"},           {31, "▱  Cards"},
        {33, "♙  Employees"},    {34, "⌂  Branches"},        {35, "▥  Reports"},
        {39, "☷  Audit Logs"},   {38, "♙  User Management"}, {40, "⚙  Settings"}};
    for (const auto &link : links) {
        QString needed = "view";
        if (link.first == 20 || link.first == 21)
            needed = "cash";
        else if (link.first == 22)
            needed = "transfer";
        else if (link.first == 33 || link.first == 38 || link.first == 40)
            needed = "admin";
        else if (link.first == 34 || link.first == 35)
            needed = "reports";
        else if (link.first == 39)
            needed = "audit";
        else if (link.first == 31 && s.user().role != "Customer")
            needed = "cards";
        if (!s.user().can(needed))
            continue;
        auto button = new QPushButton(link.second);
        button->setObjectName("nav");
        button->setProperty("screen", link.first);
        connect(button, &QPushButton::clicked, this, [this, id = link.first] { navigate(id); });
        nav->addWidget(button);
    }
    nav->addStretch();
    action(nav, "↪ Logout", [this] {
        auth_.logout();
        emit logoutRequested();
    });
    auto demo = textLabel("FICTIONAL DEMO DATA\nSQLite · Local desktop application");
    demo->setObjectName("sidebarNote");
    nav->addWidget(demo);
    row->addWidget(sidebar);
    auto right = new QWidget;
    auto outer = new QVBoxLayout(right);
    outer->setContentsMargins(30, 24, 30, 24);
    auto top = new QHBoxLayout;
    breadcrumb_ = new QLabel("Workspace / Overview");
    breadcrumb_->setObjectName("muted");
    top->addWidget(breadcrumb_);
    top->addStretch();
    auto profile = new QPushButton(s.user().name + " · " + s.user().role);
    connect(profile, &QPushButton::clicked, this, [this] { navigate(5); });
    top->addWidget(profile);
    auto notifications = new QPushButton("Notifications");
    notifications->setVisible(s.user().can("admin"));
    connect(notifications, &QPushButton::clicked, this, [this] {
        guard([this] {
            auto dialog = new QDialog(this);
            dialog->setAttribute(Qt::WA_DeleteOnClose);
            dialog->setWindowTitle("Notifications");
            dialog->resize(800, 400);
            auto l = vertical(dialog);
            auto t = records(bank_.list("notifications"), dialog);
            l->addWidget(t);
            action(l, "Mark selected as resolved", [this, t, dialog] {
                bank_.resolveNotification(t->selectedId());
                dialog->close();
            });
            dialog->show();
        });
    });
    top->addWidget(notifications);
    outer->addLayout(top);
    title_ = new QLabel;
    title_->setObjectName("pageTitle");
    outer->addWidget(title_);
    feedback_ = new QLabel;
    feedback_->setObjectName("error");
    feedback_->setWordWrap(true);
    outer->addWidget(feedback_);
    auto scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    content_ = new QWidget;
    layout_ = vertical(content_);
    scroll->setWidget(content_);
    outer->addWidget(scroll);
    auto foot = new QLabel("SecuredGlobe · Educational banking system · All data is fictional");
    foot->setObjectName("muted");
    outer->addWidget(foot);
    row->addWidget(right, 1);
    setCentralWidget(root);
    timeout_ = new QTimer(this);
    connect(timeout_, &QTimer::timeout, this, [this] {
        if (session_.expired()) {
            session_.logout();
            emit logoutRequested();
        }
    });
    timeout_->start(1000);
    qApp->installEventFilter(this);
    navigate(4);
}
bool MainWindow::eventFilter(QObject *object, QEvent *event) {
    if (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::KeyPress)
        session_.touch();
    return QMainWindow::eventFilter(object, event);
}
void MainWindow::showError(const QString &error) {
    feedback_->setText(error);
}
void MainWindow::guard(std::function<void()> handler) {
    try {
        handler();
    } catch (const std::exception &e) {
        showError(QString::fromUtf8(e.what()));
    }
}
void MainWindow::action(QVBoxLayout *l, const QString &text, std::function<void()> handler,
                        bool primary) {
    auto button = new QPushButton(text);
    if (primary)
        button->setObjectName("primary");
    connect(button, &QPushButton::clicked, this, [this, handler] { guard(handler); });
    l->addWidget(button, 0, Qt::AlignLeft);
}
QWidget *MainWindow::panel(const QString &text) {
    auto w = new QWidget;
    auto v = vertical(w);
    v->addWidget(textLabel(text));
    return w;
}
RecordTable *MainWindow::records(const TableData &data, QWidget *parent) {
    return new RecordTable(data, parent);
}
void MainWindow::navigate(int id) {
    guard([this, id] {
        auto page = build(id);
        navigation_.visit(id);
        while (auto item = layout_->takeAt(0)) {
            if (item->widget()) {
                item->widget()->hide();
                item->widget()->deleteLater();
            }
            delete item;
        }
        layout_->addWidget(page);
        title_->setText(NavigationManager::title(id));
        breadcrumb_->setText("Workspace / " + NavigationManager::title(id));
        feedback_->clear();
    });
}
QWidget *MainWindow::customerForm(bool edit) {
    bank_.require("customers");
    Record c;
    if (edit)
        c = bank_.customer(customerId_);
    auto w = new QWidget;
    auto v = vertical(w);
    auto f = new QFormLayout;
    auto name = input(f, "Full name", c["name"].toString()),
         email = input(f, "Email", c["email"].toString()),
         phone = input(f, "Phone", c["phone"].toString()),
         address = input(f, "Address", c["address"].toString());
    auto branch = optionBox(bank_.options("branches"));
    if (edit)
        branch->setCurrentIndex(branch->findData(c["branch_id"]));
    f->addRow("Branch", branch);
    v->addLayout(f);
    action(
        v, edit ? "Save customer changes" : "Create customer",
        [this, edit, name, email, phone, address, branch] {
            customerId_ = bank_.saveCustomer(edit ? customerId_ : 0, name->text(), email->text(),
                                             phone->text(), address->text(),
                                             branch->currentData().toLongLong());
            navigate(9);
        },
        true);
    v->addStretch();
    return w;
}
QWidget *MainWindow::cashForm(bool deposit) {
    bank_.require("cash");
    auto w = new QWidget;
    auto v = vertical(w);
    v->addWidget(textLabel(
        deposit
            ? "Record a cash deposit. The balance, ledger, and audit log are updated atomically."
            : "Withdraw cash from an active account. Available balance and transaction limits are "
              "enforced."));
    auto f = new QFormLayout;
    auto account = optionBox(bank_.options("accounts"));
    f->addRow("Account", account);
    auto amount = input(f, "Amount (USD)"), note = input(f, "Reference / note");
    v->addLayout(f);
    action(
        v, deposit ? "Confirm deposit" : "Confirm withdrawal",
        [this, deposit, account, amount, note] {
            auto cents = Money::parse(amount->text());
            if (QMessageBox::question(this, "Confirm cash transaction",
                                      QString("%1 %2?").arg(deposit ? "Deposit" : "Withdraw",
                                                            Money::format(cents))) !=
                QMessageBox::Yes)
                return;
            transactionId_ =
                deposit ? bank_.deposit(account->currentData().toLongLong(), cents, note->text())
                        : bank_.withdraw(account->currentData().toLongLong(), cents, note->text());
            navigate(25);
        },
        true);
    v->addStretch();
    return w;
}
QWidget *MainWindow::transactionFilters() {
    auto w = new QWidget;
    auto v = vertical(w);
    auto controls = new QHBoxLayout;
    auto from = new QDateEdit(QDate::currentDate().addMonths(-3)),
         to = new QDateEdit(QDate::currentDate());
    from->setCalendarPopup(true);
    to->setCalendarPopup(true);
    from->setAccessibleName("From date");
    to->setAccessibleName("To date");
    auto kind = new QComboBox;
    kind->addItems({"All types", "Deposit", "Withdrawal", "Transfer In", "Transfer Out",
                    "Loan Disbursement", "Loan Repayment"});
    auto min = new QLineEdit, max = new QLineEdit;
    min->setPlaceholderText("Min USD");
    max->setPlaceholderText("Max USD");
    controls->addWidget(from);
    controls->addWidget(to);
    controls->addWidget(kind);
    controls->addWidget(min);
    controls->addWidget(max);
    auto apply = new QPushButton("Apply filters");
    controls->addWidget(apply);
    v->addLayout(controls);
    auto holder = new QWidget;
    auto holderLayout = vertical(holder);
    v->addWidget(holder);
    auto update = [this, holderLayout, from, to, kind, min, max] {
        guard([this, holderLayout, from, to, kind, min, max] {
            auto data = bank_.transactions(
                from->date(), to->date(), kind->currentIndex() ? kind->currentText() : QString(),
                min->text().isEmpty() ? 0 : Money::parse(min->text()),
                max->text().isEmpty() ? std::numeric_limits<qint64>::max()
                                      : Money::parse(max->text()));
            while (auto item = holderLayout->takeAt(0)) {
                if (item->widget()) {
                    item->widget()->hide();
                    item->widget()->deleteLater();
                }
                delete item;
            }
            auto t = records(data);
            holderLayout->addWidget(t);
            connect(t, &RecordTable::activated, this, [this](qint64 id) {
                transactionId_ = id;
                navigate(23);
            });
        });
    };
    connect(apply, &QPushButton::clicked, this, update);
    update();
    return w;
}
QWidget *MainWindow::reports(bool financial) {
    bank_.require("reports");
    auto w = new QWidget;
    auto v = vertical(w);
    auto filters = new QHBoxLayout;
    auto type = new QComboBox;
    type->addItems(financial ? QStringList{"financial", "transactions", "branches"}
                             : QStringList{"customers", "accounts"});
    auto from = new QDateEdit(QDate::currentDate().addMonths(-3)),
         to = new QDateEdit(QDate::currentDate());
    from->setCalendarPopup(true);
    to->setCalendarPopup(true);
    filters->addWidget(type);
    filters->addWidget(new QLabel("From"));
    filters->addWidget(from);
    filters->addWidget(new QLabel("To"));
    filters->addWidget(to);
    auto apply = new QPushButton("Generate report");
    filters->addWidget(apply);
    v->addLayout(filters);
    v->addWidget(textLabel("Date range applies to financial and transaction reports. Customer, "
                           "account, and branch reports show current records."));
    auto holder = new QWidget;
    auto l = vertical(holder);
    v->addWidget(holder);
    auto update = [this, l, type, from, to] {
        guard([this, l, type, from, to] {
            if (from->date() > to->date())
                throw std::runtime_error("Start date must precede end date");
            auto data = bank_.report(type->currentText(), from->date(), to->date());
            while (auto item = l->takeAt(0)) {
                if (item->widget()) {
                    item->widget()->hide();
                    item->widget()->deleteLater();
                }
                delete item;
            }
            l->addWidget(records(data));
        });
    };
    connect(apply, &QPushButton::clicked, this, update);
    update();
    if (financial)
        action(v, "Customer & account reports", [this] { navigate(36); });
    return w;
}
QWidget *MainWindow::build(int id) {
    if (id == 4) {
        auto d = bank_.dashboard();
        auto w = new QWidget;
        auto v = vertical(w);
        v->addWidget(textLabel("Welcome back, " + session_.user().name +
                               ". Here is your banking overview."));
        auto cards = new QHBoxLayout;
        for (const auto &pair : QVector<QPair<QString, QString>>{
                 {"Total balance", Money::format(d["balance"].toLongLong())},
                 {"Customers", d["customers"].toString()},
                 {"Active accounts", d["active"].toString()},
                 {"Pending loans", d["pending"].toString()}}) {
            auto label = textLabel(pair.first + "\n\n" + pair.second);
            label->setObjectName("statCard");
            cards->addWidget(label);
        }
        v->addLayout(cards);
        v->addWidget(new ActivityChart(bank_.monthlyActivity()));
        v->addWidget(
            textLabel("Cash deposits: " + Money::format(d["deposits"].toLongLong()) +
                      " · Cash withdrawals: " + Money::format(d["withdrawals"].toLongLong())));
        v->addWidget(textLabel("Loan decisions · Approved: " + d["Approved"].toString() +
                               " · Rejected: " + d["Rejected"].toString() +
                               " · Repaid: " + d["Repaid"].toString()));
        if (session_.user().can("reports")) {
            QStringList summaries;
            for (const auto &row : bank_.list("branches").rows)
                summaries << row[1].toString() + ": " + row[3].toString() + " customers";
            v->addWidget(textLabel("Branch activity · " + summaries.join(" · ")));
        }
        auto recent = bank_.transactions(QDate(2000, 1, 1), QDate::currentDate());
        if (recent.rows.size() > 5)
            recent.rows.resize(5);
        auto t = records(recent);
        connect(t, &RecordTable::activated, this, [this](qint64 tid) {
            transactionId_ = tid;
            navigate(23);
        });
        v->addWidget(t);
        if (session_.user().can("reports"))
            action(v, "Open transaction analytics", [this] { navigate(37); });
        return w;
    }
    if (id == 5) {
        auto p = bank_.profile();
        auto w = panel("Name: " + p["name"].toString() + "\nUsername: " + p["username"].toString() +
                       "\nRole: " + p["role"].toString() +
                       "\nSession timeout: 15 minutes of inactivity");
        auto v = qobject_cast<QVBoxLayout *>(w->layout());
        action(v, "Change password", [this] { navigate(6); }, true);
        if (p["customer_id"].toLongLong()) {
            customerId_ = p["customer_id"].toLongLong();
            action(v, "My customer profile", [this] { navigate(9); });
        }
        return w;
    }
    if (id == 6) {
        auto w = panel("Use 12–128 characters with uppercase, lowercase, a number, and a symbol.");
        auto v = qobject_cast<QVBoxLayout *>(w->layout());
        auto f = new QFormLayout;
        auto old = input(f, "Current password"), next = input(f, "New password"),
             confirm = input(f, "Confirm new password");
        for (auto p : {old, next, confirm}) {
            p->setEchoMode(QLineEdit::Password);
            p->setMaxLength(128);
        }
        v->addLayout(f);
        action(
            v, "Update password",
            [this, old, next, confirm] {
                if (next->text() != confirm->text())
                    throw std::runtime_error("New passwords do not match");
                auth_.changePassword(old->text(), next->text());
                old->clear();
                next->clear();
                confirm->clear();
                navigate(5);
            },
            true);
        return w;
    }
    if (id == 7) {
        auto w = new QWidget;
        auto v = vertical(w);
        auto t = records(bank_.list("customers"));
        v->addWidget(t);
        connect(t, &RecordTable::activated, this, [this](qint64 cid) {
            customerId_ = cid;
            navigate(9);
        });
        if (session_.user().can("customers"))
            action(v, "＋ Add customer", [this] { navigate(8); }, true);
        action(v, "View selected customer", [this, t] {
            customerId_ = t->selectedId();
            navigate(9);
        });
        return w;
    }
    if (id == 8)
        return customerForm(false);
    if (id == 9) {
        auto c = bank_.customer(customerId_);
        auto w =
            panel(QString("Customer #%1\n%2\n%3\n%4\n%5\nBranch: %6\nKYC: %7")
                      .arg(customerId_)
                      .arg(c["name"].toString(), c["email"].toString(), c["phone"].toString(),
                           c["address"].toString(), c["branch"].toString(), c["kyc"].toString()));
        auto v = qobject_cast<QVBoxLayout *>(w->layout());
        if (session_.user().can("customers"))
            action(v, "Edit customer", [this] { navigate(10); });
        if (session_.user().can("approve"))
            action(v, "Review customer KYC", [this] { navigate(11); });
        action(v, "Account history", [this] { navigate(12); }, true);
        return w;
    }
    if (id == 10)
        return customerForm(true);
    if (id == 11) {
        bank_.require("approve");
        auto c = bank_.customer(customerId_);
        auto w = panel("Verify fictional customer: " + c["name"].toString() +
                       "\nCurrent status: " + c["kyc"].toString());
        auto v = qobject_cast<QVBoxLayout *>(w->layout());
        auto f = new QFormLayout;
        auto reference = input(f, "Document reference", c["document_reference"].toString());
        auto status = new QComboBox;
        status->addItems({"Pending", "Verified", "Rejected"});
        status->setCurrentText(c["kyc"].toString());
        f->addRow("Review outcome", status);
        v->addLayout(f);
        action(
            v, "Save KYC decision",
            [this, reference, status] {
                bank_.verifyCustomer(customerId_, status->currentText(), reference->text());
                navigate(9);
            },
            true);
        return w;
    }
    if (id == 12) {
        bank_.customer(customerId_);
        auto w = new QWidget;
        auto v = vertical(w);
        v->addWidget(
            textLabel("Complete account activity for customer #" + QString::number(customerId_)));
        auto t = records(bank_.customerHistory(customerId_));
        v->addWidget(t);
        connect(t, &RecordTable::activated, this, [this](qint64 tid) {
            transactionId_ = tid;
            navigate(23);
        });
        return w;
    }
    if (id == 13) {
        auto w = new QWidget;
        auto v = vertical(w);
        auto all = bank_.list("accounts");
        auto filters = new QHBoxLayout;
        auto type = new QComboBox, status = new QComboBox;
        type->addItems({"All types", "Savings", "Current"});
        status->addItems({"All statuses", "Active", "Frozen", "Closed"});
        filters->addWidget(type);
        filters->addWidget(status);
        v->addLayout(filters);
        auto holder = new QWidget;
        auto l = vertical(holder);
        v->addWidget(holder);
        auto update = [this, l, type, status, all] {
            TableData filtered;
            filtered.headers = all.headers;
            for (const auto &row : all.rows)
                if ((type->currentIndex() == 0 || row[3].toString() == type->currentText()) &&
                    (status->currentIndex() == 0 || row[5].toString() == status->currentText()))
                    filtered.rows << row;
            while (auto item = l->takeAt(0)) {
                if (item->widget()) {
                    item->widget()->hide();
                    item->widget()->deleteLater();
                }
                delete item;
            }
            auto t = records(filtered);
            l->addWidget(t);
            connect(t, &RecordTable::activated, this, [this](qint64 aid) {
                accountId_ = aid;
                navigate(15);
            });
            action(l, "Manage selected account", [this, t] {
                accountId_ = t->selectedId();
                navigate(15);
            });
        };
        connect(type, &QComboBox::currentTextChanged, this, [update] { update(); });
        connect(status, &QComboBox::currentTextChanged, this, [update] { update(); });
        update();
        if (session_.user().can("accounts"))
            action(v, "＋ Open account", [this] { navigate(14); }, true);
        return w;
    }
    if (id == 14) {
        bank_.require("accounts");
        auto w = panel("Open a zero-balance account for a verified customer. Add funds through the "
                       "deposit workflow.");
        auto v = qobject_cast<QVBoxLayout *>(w->layout());
        auto f = new QFormLayout;
        auto c = optionBox(bank_.options("customers")),
             type = optionBox(bank_.options("account_types"));
        f->addRow("Customer", c);
        f->addRow("Account type", type);
        auto nickname = input(f, "Account nickname");
        v->addLayout(f);
        action(
            v, "Open new account",
            [this, c, type, nickname] {
                accountId_ = bank_.openAccount(c->currentData().toLongLong(),
                                               type->currentData().toLongLong(), nickname->text());
                navigate(15);
            },
            true);
        return w;
    }
    if (id == 15) {
        auto a = bank_.account(accountId_);
        auto w = panel(QString("%1\n%2 · %3\nBalance: %4\nStatus: %5\nCustomer #%6")
                           .arg(a.number, a.type, a.nickname, Money::format(a.balance), a.status)
                           .arg(a.customerId));
        auto v = qobject_cast<QVBoxLayout *>(w->layout());
        if (session_.user().can("accounts"))
            action(v, "Edit account information", [this] { navigate(16); });
        if (session_.user().can("approve")) {
            action(v, "Freeze / unfreeze", [this] { navigate(17); });
            action(v, "Close account", [this] { navigate(18); });
        }
        action(v, "Customer account history", [this, a] {
            customerId_ = a.customerId;
            navigate(12);
        });
        return w;
    }
    if (id == 16) {
        bank_.require("accounts");
        auto a = bank_.account(accountId_);
        auto w = panel("Edit nickname and account type for " + a.number);
        auto v = qobject_cast<QVBoxLayout *>(w->layout());
        auto f = new QFormLayout;
        auto nickname = input(f, "Nickname", a.nickname);
        auto type = optionBox(bank_.options("account_types"));
        type->setCurrentText(a.type);
        f->addRow("Type", type);
        v->addLayout(f);
        action(
            v, "Save account",
            [this, nickname, type] {
                bank_.editAccount(accountId_, nickname->text(), type->currentData().toLongLong());
                navigate(15);
            },
            true);
        return w;
    }
    if (id == 17) {
        bank_.require("approve");
        auto a = bank_.account(accountId_);
        auto w = panel("Account " + a.number + "\nCurrent status: " + a.status +
                       "\nFreezing prevents deposits, withdrawals, transfers, and repayments. "
                       "Attached cards are blocked.");
        auto v = qobject_cast<QVBoxLayout *>(w->layout());
        if (a.status != "Closed")
            action(
                v, a.status == "Frozen" ? "Unfreeze account" : "Freeze account",
                [this, a] {
                    if (QMessageBox::question(this, "Confirm status change",
                                              "Update this account status?") != QMessageBox::Yes)
                        return;
                    bank_.setAccountStatus(a.id, a.status == "Frozen" ? "Active" : "Frozen");
                    navigate(15);
                },
                true);
        return w;
    }
    if (id == 18) {
        bank_.require("approve");
        auto a = bank_.account(accountId_);
        auto w = panel("Close account " + a.number + "\nBalance: " + Money::format(a.balance) +
                       "\nClosure requires zero balance and no pending or approved loans. A closed "
                       "account cannot be reopened.");
        auto v = qobject_cast<QVBoxLayout *>(w->layout());
        if (a.status != "Closed")
            action(v, "Confirm permanent account closure", [this] {
                if (QMessageBox::warning(this, "Close account", "Close this demonstration account?",
                                         QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
                    return;
                bank_.setAccountStatus(accountId_, "Closed");
                navigate(15);
            });
        return w;
    }
    if (id == 19) {
        auto w = new QWidget;
        auto v = vertical(w);
        auto t = records(bank_.transactions(QDate(2000, 1, 1), QDate::currentDate()));
        v->addWidget(t);
        connect(t, &RecordTable::activated, this, [this](qint64 tid) {
            transactionId_ = tid;
            navigate(23);
        });
        action(v, "View selected transaction", [this, t] {
            transactionId_ = t->selectedId();
            navigate(23);
        });
        action(v, "Date, type & amount filters", [this] { navigate(24); });
        return w;
    }
    if (id == 20)
        return cashForm(true);
    if (id == 21)
        return cashForm(false);
    if (id == 22) {
        bank_.require("transfer");
        auto w = panel("Transfer between active accounts. Debit, credit, ledger entries, and audit "
                       "record commit together. Reusing a request key is rejected.");
        auto v = qobject_cast<QVBoxLayout *>(w->layout());
        auto f = new QFormLayout;
        auto from = optionBox(bank_.options("accounts"));
        f->addRow("From account", from);
        auto recipient = input(f, "Recipient account ID"), amount = input(f, "Amount (USD)"),
             note = input(f, "Reference / note");
        transferKey_ = QUuid::createUuid().toString(QUuid::WithoutBraces);
        auto key = input(f, "Idempotency key", transferKey_);
        key->setReadOnly(true);
        v->addLayout(f);
        action(
            v, "Confirm transfer",
            [this, from, recipient, amount, note, key] {
                bool ok = false;
                auto target = recipient->text().toLongLong(&ok);
                if (!ok)
                    throw std::runtime_error("Enter a numeric recipient account ID");
                auto cents = Money::parse(amount->text());
                if (QMessageBox::question(this, "Confirm transfer",
                                          QString("Send %1 to account ID %2?")
                                              .arg(Money::format(cents))
                                              .arg(target)) != QMessageBox::Yes)
                    return;
                transactionId_ = bank_.transfer(from->currentData().toLongLong(), target, cents,
                                                key->text(), note->text());
                navigate(25);
            },
            true);
        return w;
    }
    if (id == 23) {
        auto t = bank_.transaction(transactionId_);
        auto w = panel(QString("Reference: %1\nAccount: %2\nCustomer: %3\nType: %4\nAmount: "
                               "%5\nDate (UTC): %6\nNote: %7")
                           .arg(t["reference"].toString(), t["account"].toString(),
                                t["customer"].toString(), t["kind"].toString(),
                                Money::format(t["amount"].toLongLong()), t["created_at"].toString(),
                                t["note"].toString()));
        auto v = qobject_cast<QVBoxLayout *>(w->layout());
        action(v, "Open transaction receipt", [this] { navigate(25); }, true);
        return w;
    }
    if (id == 24)
        return transactionFilters();
    if (id == 25) {
        auto w = new QWidget;
        auto v = vertical(w);
        auto browser = new QTextBrowser;
        browser->setHtml(receiptHtml(transactionId_));
        v->addWidget(browser);
        action(v, "Export receipt PDF", [this] { printReceipt(transactionId_, true); }, true);
        action(v, "Print receipt", [this] { printReceipt(transactionId_, false); });
        return w;
    }
    if (id == 26) {
        auto w = new QWidget;
        auto v = vertical(w);
        auto all = bank_.list("loans");
        auto status = new QComboBox;
        status->addItems({"All statuses", "Pending", "Approved", "Rejected", "Repaid"});
        v->addWidget(status);
        auto holder = new QWidget;
        auto l = vertical(holder);
        v->addWidget(holder);
        auto update = [this, status, all, l] {
            TableData d;
            d.headers = all.headers;
            for (const auto &row : all.rows)
                if (status->currentIndex() == 0 || row[4].toString() == status->currentText())
                    d.rows << row;
            while (auto item = l->takeAt(0)) {
                if (item->widget()) {
                    item->widget()->hide();
                    item->widget()->deleteLater();
                }
                delete item;
            }
            auto t = records(d);
            l->addWidget(t);
            connect(t, &RecordTable::activated, this, [this](qint64 lid) {
                loanId_ = lid;
                navigate(30);
            });
            action(l, "Loan details & schedule", [this, t] {
                loanId_ = t->selectedId();
                navigate(30);
            });
            if (session_.user().can("approve"))
                action(l, "Review selected application", [this, t] {
                    loanId_ = t->selectedId();
                    navigate(28);
                });
        };
        connect(status, &QComboBox::currentTextChanged, this, [update] { update(); });
        update();
        if (session_.user().can("loans"))
            action(v, "＋ Apply for loan", [this] { navigate(27); }, true);
        return w;
    }
    if (id == 27) {
        bank_.require("loans");
        auto w = panel("Apply for a fixed-rate loan at 8.50% annually. KYC verification and an "
                       "active account are required.");
        auto v = qobject_cast<QVBoxLayout *>(w->layout());
        auto f = new QFormLayout;
        auto account = optionBox(bank_.options("accounts"));
        f->addRow("Disbursement account", account);
        auto principal = input(f, "Principal (USD)"), purpose = input(f, "Purpose");
        auto months = new QSpinBox;
        months->setRange(1, 360);
        months->setValue(24);
        f->addRow("Term in months", months);
        v->addLayout(f);
        auto estimate = new QLabel;
        v->addWidget(estimate);
        action(v, "Calculate repayment", [principal, months, estimate] {
            auto p = Money::parse(principal->text());
            estimate->setText(
                "Monthly payment: " +
                Money::format(BankingService::monthlyPayment(p, months->value(), 850)) +
                "\nFinal installment is adjusted for rounding. Interest uses a reducing balance.");
        });
        action(
            v, "Submit application",
            [this, account, principal, purpose, months] {
                loanId_ = bank_.applyLoan(account->currentData().toLongLong(),
                                          Money::parse(principal->text()), months->value(), 850,
                                          purpose->text());
                navigate(30);
            },
            true);
        return w;
    }
    if (id == 28) {
        bank_.require("approve");
        auto l = bank_.loan(loanId_);
        auto w =
            panel("Application #" + QString::number(loanId_) + " · " + l["customer"].toString() +
                  "\nPrincipal: " + Money::format(l["principal"].toLongLong()) +
                  "\nPurpose: " + l["purpose"].toString() + "\nStatus: " + l["status"].toString());
        auto v = qobject_cast<QVBoxLayout *>(w->layout());
        auto f = new QFormLayout;
        auto reason = input(f, "Decision reason");
        v->addLayout(f);
        if (l["status"].toString() == "Pending") {
            action(
                v, "Approve & disburse loan",
                [this, reason] {
                    if (QMessageBox::question(
                            this, "Loan approval",
                            "Approve and credit this loan to the customer's account?") !=
                        QMessageBox::Yes)
                        return;
                    bank_.reviewLoan(loanId_, true, reason->text());
                    navigate(30);
                },
                true);
            action(v, "Reject application", [this, reason] {
                bank_.reviewLoan(loanId_, false, reason->text());
                navigate(30);
            });
        }
        return w;
    }
    if (id == 29) {
        bank_.require("loans");
        auto l = bank_.loan(loanId_);
        auto w = panel("Loan #" + QString::number(loanId_) + "\nOutstanding: " +
                       Money::format(l["total_due"].toLongLong() - l["paid"].toLongLong()) +
                       "\nPayment is debited from the linked disbursement account.");
        auto v = qobject_cast<QVBoxLayout *>(w->layout());
        auto f = new QFormLayout;
        auto amount = input(f, "Repayment amount (USD)");
        v->addLayout(f);
        action(
            v, "Record repayment",
            [this, amount] {
                auto cents = Money::parse(amount->text());
                if (QMessageBox::question(this, "Confirm repayment",
                                          Money::format(cents) +
                                              " will be debited from the linked account.") !=
                    QMessageBox::Yes)
                    return;
                bank_.repayLoan(loanId_, cents);
                navigate(30);
            },
            true);
        return w;
    }
    if (id == 30) {
        auto l = bank_.loan(loanId_);
        auto w = new QWidget;
        auto v = vertical(w);
        v->addWidget(textLabel(
            QString("Loan #%1 · %2 · %3\nPrincipal: %4 · Outstanding: %5\nAnnual rate: %6% · Term: "
                    "%7 months\n%8")
                .arg(loanId_)
                .arg(l["customer"].toString(), l["status"].toString(),
                     Money::format(l["principal"].toLongLong()),
                     Money::format(l["total_due"].toLongLong() - l["paid"].toLongLong()))
                .arg(l["annual_rate_bps"].toDouble() / 100, 0, 'f', 2)
                .arg(l["term_months"].toInt())
                .arg(l["approved_at"].isNull() ? "Indicative due dates until approval"
                                               : "Due dates are based on approval date")));
        v->addWidget(records(bank_.schedule(loanId_)));
        if (session_.user().can("approve"))
            action(v, "Approval / rejection", [this] { navigate(28); });
        if (session_.user().can("loans") && l["status"].toString() == "Approved")
            action(v, "Make repayment", [this] { navigate(29); }, true);
        return w;
    }
    if (id == 31) {
        auto w = new QWidget;
        auto v = vertical(w);
        auto t = records(bank_.list("cards"));
        v->addWidget(t);
        connect(t, &RecordTable::activated, this, [this](qint64 cid) {
            cardId_ = cid;
            navigate(32);
        });
        if (session_.user().can("cards"))
            action(
                v, "Issue or manage cards",
                [this, t] {
                    if (!t->filtered().rows.isEmpty())
                        cardId_ = t->selectedId();
                    navigate(32);
                },
                true);
        return w;
    }
    if (id == 32) {
        bank_.require("cards");
        auto w = panel("Masked demonstration card identifiers only. No PIN, CVV, or real card "
                       "number is generated.");
        auto v = qobject_cast<QVBoxLayout *>(w->layout());
        auto f = new QFormLayout;
        auto account = optionBox(bank_.options("accounts"));
        f->addRow("Issue for account", account);
        v->addLayout(f);
        action(
            v, "Issue demo debit card",
            [this, account] {
                cardId_ = bank_.issueCard(account->currentData().toLongLong());
                navigate(31);
            },
            true);
        auto table = records(bank_.list("cards"));
        v->addWidget(table);
        action(v, "Block selected card", [this, table] {
            bank_.blockCard(table->selectedId(), true);
            navigate(32);
        });
        action(v, "Unblock selected card", [this, table] {
            bank_.blockCard(table->selectedId(), false);
            navigate(32);
        });
        return w;
    }
    if (id == 33) {
        bank_.require("admin");
        auto w = new QWidget;
        auto v = vertical(w);
        auto t = records(bank_.list("employees"));
        v->addWidget(t);
        action(v, "＋ Add employee", [this] { editEmployee(); }, true);
        action(v, "Edit selected employee", [this, t] { editEmployee(t->selectedId()); });
        return w;
    }
    if (id == 34) {
        auto w = new QWidget;
        auto v = vertical(w);
        auto t = records(bank_.list("branches"));
        v->addWidget(t);
        if (session_.user().can("admin")) {
            action(v, "＋ Add branch", [this] { editBranch(); }, true);
            action(v, "Edit selected branch", [this, t] { editBranch(t->selectedId()); });
        }
        return w;
    }
    if (id == 35)
        return reports(true);
    if (id == 36)
        return reports(false);
    if (id == 37) {
        bank_.require("reports");
        auto w = new QWidget;
        auto v = vertical(w);
        v->addWidget(textLabel("Monthly inflows and outflows, derived from the live transaction "
                               "ledger. Internal transfers appear on both sides."));
        auto data = bank_.monthlyActivity();
        v->addWidget(new ActivityChart(data));
        v->addWidget(records(data));
        return w;
    }
    if (id == 38) {
        bank_.require("admin");
        auto w = new QWidget;
        auto v = vertical(w);
        auto t = records(bank_.list("users"));
        v->addWidget(t);
        action(v, "＋ Create login", [this] { createUser(); }, true);
        action(v, "Change selected role", [this, t] {
            auto uid = t->selectedId();
            FormDialog d("Change role", "Changes take effect when the user signs in again.", this);
            d.choices("role", "Role",
                      QStringList{"Administrator", "Bank Manager", "Bank Teller", "Customer"});
            d.submit("Update role", [this, uid](const QVariantMap &v) {
                auth_.changeRole(uid, v["role"].toString());
            });
            if (d.exec() == QDialog::Accepted)
                navigate(38);
        });
        action(v, "Admin-assisted password reset", [this, t] {
            auto uid = t->selectedId();
            FormDialog d("Reset password",
                         "Verify the user's identity first. Share the temporary password through "
                         "an approved channel. A password change is required at next login.",
                         this);
            d.text("password", "Temporary password", {}, true);
            d.submit("Reset password", [this, uid](const QVariantMap &v) {
                auth_.resetPassword(uid, v["password"].toString());
            });
            if (d.exec() == QDialog::Accepted)
                navigate(38);
        });
        action(v, "Enable / disable selected login", [this, t] {
            auto row = t->selectedRow();
            bool active = row[4].toBool();
            if (QMessageBox::question(this, "Change login status",
                                      active ? "Disable this user login?"
                                             : "Enable this user login?") != QMessageBox::Yes)
                return;
            auth_.setActive(row[0].toLongLong(), !active);
            navigate(38);
        });
        return w;
    }
    if (id == 39) {
        auto w = new QWidget;
        auto v = vertical(w);
        v->addWidget(textLabel("Audit events are stored with actor, action, detail, and UTC "
                               "timestamp. Use search to filter and export the filtered records."));
        v->addWidget(records(bank_.list("audit")));
        return w;
    }
    if (id == 40) {
        bank_.require("admin");
        auto w = panel("Application settings\nDatabase changes persist immediately. The inactivity "
                       "timeout is 15 minutes.");
        auto v = qobject_cast<QVBoxLayout *>(w->layout());
        auto f = new QFormLayout;
        auto amount = input(f, "Maximum withdrawal / transfer (USD)",
                            QString::number(bank_.limit() / 100) + "." +
                                QString::number(bank_.limit() % 100).rightJustified(2, '0'));
        v->addLayout(f);
        action(
            v, "Save limit",
            [this, amount] {
                bank_.setLimit(Money::parse(amount->text()));
                feedback_->setText("Transaction limit saved.");
            },
            true);
        action(v, "Create SQLite backup", [this] {
            auto path = QFileDialog::getSaveFileName(this, "Back up database to a new file", {},
                                                     "SQLite (*.sqlite)");
            if (path.isEmpty())
                return;
            bank_.backup(path);
            QMessageBox::information(this, "Backup created", path);
        });
        v->addWidget(textLabel(
            "Restore: close the application, preserve the current database, and copy a backup over "
            "the data file. The README documents the platform-specific location.\n\nEducational "
            "system only. No certification or real banking integration is claimed."));
        return w;
    }
    throw std::runtime_error("This screen is not part of the authenticated workspace");
}
void MainWindow::editEmployee(qint64 id) {
    Record record;
    if (id)
        record = bank_.employee(id);
    FormDialog d(id ? "Edit employee" : "Add employee",
                 "Employees must reference an existing staff login. Create the login in User "
                 "Management first.",
                 this);
    d.choices("user", "Staff login", bank_.options("staff"), record["user_id"].toLongLong());
    d.choices("branch", "Branch", bank_.options("branches"), record["branch_id"].toLongLong());
    d.text("title", "Job title", record["title"].toString());
    d.submit("Save employee", [this, id](const QVariantMap &v) {
        bank_.saveEmployee(id, v["user"].toLongLong(), v["branch"].toLongLong(),
                           v["title"].toString());
    });
    if (d.exec() == QDialog::Accepted)
        navigate(33);
}
void MainWindow::editBranch(qint64 id) {
    Record record;
    if (id)
        record = bank_.branch(id);
    FormDialog d(id ? "Edit branch" : "Add branch", "Manage a branch's display name and city.",
                 this);
    d.text("name", "Branch name", record["name"].toString());
    d.text("city", "City", record["city"].toString());
    d.submit("Save branch", [this, id](const QVariantMap &v) {
        bank_.saveBranch(id, v["name"].toString(), v["city"].toString());
    });
    if (d.exec() == QDialog::Accepted)
        navigate(34);
}
void MainWindow::createUser() {
    FormDialog d("Create login",
                 "New logins must change their temporary password. Staff logins use customer ID 0.",
                 this);
    d.text("username", "Username");
    d.text("name", "Display name");
    d.choices("role", "Role",
              QStringList{"Bank Teller", "Bank Manager", "Administrator", "Customer"});
    d.text("customer", "Linked customer ID (0 for staff)", "0");
    d.text("password", "Temporary password", {}, true);
    d.submit("Create login", [this](const QVariantMap &v) {
        bool ok = false;
        auto cid = v["customer"].toString().toLongLong(&ok);
        if (!ok)
            throw std::runtime_error("Invalid customer ID");
        auth_.createUser(v["username"].toString(), v["name"].toString(), v["role"].toString(),
                         v["password"].toString(), cid);
    });
    if (d.exec() == QDialog::Accepted)
        navigate(38);
}
void MainWindow::changePassword() {
    navigate(6);
}
QString MainWindow::receiptHtml(qint64 id) const {
    auto t = bank_.transaction(id);
    QString h = "<h1 style='color:#177c70'>◈ SecuredGlobe</h1><h2>Transaction receipt</h2><table "
                "cellpadding='8'>";
    for (const auto &pair :
         QVector<QPair<QString, QString>>{{"Reference", t["reference"].toString()},
                                          {"Account", t["account"].toString()},
                                          {"Customer", t["customer"].toString()},
                                          {"Type", t["kind"].toString()},
                                          {"Amount", Money::format(t["amount"].toLongLong())},
                                          {"Date (UTC)", t["created_at"].toString()},
                                          {"Note", t["note"].toString()}})
        h += "<tr><th align='left'>" + pair.first.toHtmlEscaped() + "</th><td>" +
             pair.second.toHtmlEscaped() + "</td></tr>";
    return h +
           "</table><p>Completed · Fictional educational transaction. No real funds processed.</p>";
}
void MainWindow::printReceipt(qint64 id, bool pdf) {
    QPrinter printer(QPrinter::HighResolution);
    if (pdf) {
        auto path = QFileDialog::getSaveFileName(this, "Export receipt", {}, "PDF (*.pdf)");
        if (path.isEmpty())
            return;
        printer.setOutputFormat(QPrinter::PdfFormat);
        printer.setOutputFileName(path);
    } else {
        QPrintDialog dialog(&printer, this);
        if (dialog.exec() != QDialog::Accepted)
            return;
    }
    QTextDocument doc;
    doc.setHtml(receiptHtml(id));
    doc.print(&printer);
}
bool MainWindow::smokeAllScreens() {
    try {
        for (int id = 4; id <= 40; ++id) {
            std::unique_ptr<QWidget> page(build(id));
            page->resize(1000, 700);
            page->show();
            qApp->processEvents();
            qInfo() << "Constructed screen" << id << NavigationManager::title(id);
        }
        navigate(4);
        show();
        qApp->processEvents();
        auto screenshot = qEnvironmentVariable("SG_SCREENSHOT");
        if (!screenshot.isEmpty() && !grab().save(screenshot))
            throw std::runtime_error("Could not save screenshot");
        return true;
    } catch (const std::exception &e) {
        qCritical() << e.what();
        return false;
    }
}
