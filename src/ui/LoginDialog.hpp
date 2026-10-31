#pragma once
#include "services/AuthService.hpp"
#include <QDialog>
class LoginDialog final : public QDialog {
  public:
    explicit LoginDialog(AuthService &auth, QWidget *parent = nullptr);
};
