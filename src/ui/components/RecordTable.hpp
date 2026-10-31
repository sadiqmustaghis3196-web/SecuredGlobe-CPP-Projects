#pragma once
#include "repositories/BankRepository.hpp"
#include <QWidget>
class QTableWidget;
class QLineEdit;
class QLabel;
class RecordTable final : public QWidget {
    Q_OBJECT
  public:
    RecordTable(TableData data, QWidget *parent = nullptr);
    qint64 selectedId() const;
    QVariantList selectedRow() const;
    void exportCsv();
    void exportPdf();
    void print();
    TableData filtered() const;
  signals:
    void activated(qint64 id);

  private:
    TableData data_;
    QTableWidget *table_;
    QLineEdit *search_;
    QLabel *count_;
    int page_ = 0;
    void populate();
    QString html() const;
};
