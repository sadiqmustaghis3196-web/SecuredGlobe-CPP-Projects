#pragma once
#include "repositories/BankRepository.hpp"
#include <QWidget>
class ActivityChart final : public QWidget {
  public:
    explicit ActivityChart(TableData data, QWidget *parent = nullptr);

  protected:
    void paintEvent(QPaintEvent *event) override;

  private:
    TableData data_;
};
