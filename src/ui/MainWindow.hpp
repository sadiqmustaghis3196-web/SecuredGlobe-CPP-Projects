#pragma once
#include "core/NavigationManager.hpp"
#include "services/AuthService.hpp"
#include "services/BankingService.hpp"
#include <QMainWindow>
#include <functional>
class QVBoxLayout;
class QLabel;
class QStackedWidget;
class QTimer;
class RecordTable;
class MainWindow final : public QMainWindow {
    Q_OBJECT
  public:
    MainWindow(BankingService &bank, AuthService &auth, SessionManager &session);
    void navigate(int id);
    bool smokeAllScreens();
  signals:
    void logoutRequested();

  protected:
    bool eventFilter(QObject *object, QEvent *event) override;

  private:
    BankingService &bank_;
    AuthService &auth_;
    SessionManager &session_;
    NavigationManager navigation_;
    QWidget *content_;
    QVBoxLayout *layout_;
    QLabel *title_;
    QLabel *breadcrumb_;
    QLabel *feedback_;
    QTimer *timeout_;
    qint64 customerId_ = 1, accountId_ = 1, transactionId_ = 1, loanId_ = 1, cardId_ = 1;
    QString transferKey_;
    QWidget *build(int id);
    QWidget *customerForm(bool edit);
    QWidget *cashForm(bool deposit);
    QWidget *transactionFilters();
    QWidget *reports(bool financial);
    QWidget *panel(const QString &text);
    RecordTable *records(const TableData &data, QWidget *parent = nullptr);
    void action(QVBoxLayout *layout, const QString &text, std::function<void()> handler,
                bool primary = false);
    void guard(std::function<void()> handler);
    void showError(const QString &error);
    void editEmployee(qint64 id = 0);
    void editBranch(qint64 id = 0);
    void createUser();
    void changePassword();
    void printReceipt(qint64 id, bool pdf);
    QString receiptHtml(qint64 id) const;
};
