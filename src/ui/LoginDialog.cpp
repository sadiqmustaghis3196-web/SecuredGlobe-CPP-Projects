#include "LoginDialog.hpp"
#include "components/FormDialog.hpp"
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>
LoginDialog::LoginDialog(AuthService &auth, QWidget *p) : QDialog(p) {
    setWindowTitle("SecuredGlobe · Sign in");
    setMinimumSize(470, 460);
    auto l = new QVBoxLayout(this);
    l->setContentsMargins(40, 35, 40, 35);
    auto brand = new QLabel("◈ SecuredGlobe");
    brand->setObjectName("pageTitle");
    l->addWidget(brand);
    auto desc = new QLabel("Welcome back\nSign in to your banking workspace.");
    desc->setObjectName("muted");
    l->addWidget(desc);
    l->addWidget(new QLabel("Username"));
    auto user = new QLineEdit;
    user->setObjectName("username");
    user->setAccessibleName("Username");
    l->addWidget(user);
    l->addWidget(new QLabel("Password"));
    auto password = new QLineEdit;
    password->setObjectName("password");
    password->setAccessibleName("Password");
    password->setEchoMode(QLineEdit::Password);
    password->setMaxLength(128);
    l->addWidget(password);
    auto error = new QLabel;
    error->setObjectName("error");
    error->setWordWrap(true);
    l->addWidget(error);
    auto login = new QPushButton("Sign in →");
    login->setObjectName("primary");
    login->setDefault(true);
    l->addWidget(login);
    auto forgot = new QPushButton("Forgot password? Request administrator assistance");
    l->addWidget(forgot);
    auto note = new QLabel("Educational demo · All data is fictional\nFirst use: admin / "
                           "DemoBank!2026\nA new password is required after sign-in.");
    note->setObjectName("muted");
    note->setWordWrap(true);
    l->addWidget(note);
    connect(login, &QPushButton::clicked, this, [&, user, password, error, login] {
        login->setEnabled(false);
        try {
            auth.login(user->text(), password->text());
            password->clear();
            accept();
        } catch (const std::exception &e) {
            error->setText(QString::fromUtf8(e.what()));
            password->clear();
        }
        login->setEnabled(true);
    });
    connect(forgot, &QPushButton::clicked, this, [this, &auth] {
        FormDialog reset("Password assistance",
                         "The administrator will verify your identity and provide a temporary "
                         "password through your approved channel.",
                         this);
        reset.text("username", "Username");
        reset.submit("Request reset", [&auth](const QVariantMap &v) {
            auth.requestReset(v["username"].toString());
        });
        if (reset.exec() == QDialog::Accepted)
            QMessageBox::information(
                this, "Request recorded",
                "If this login exists, an administrator can assist. No password has been changed.");
    });
}
