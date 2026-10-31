#include "core/Database.hpp"
#include "core/SessionManager.hpp"
#include "repositories/BankRepository.hpp"
#include "services/AuthService.hpp"
#include "services/BankingService.hpp"
#include "services/SeedService.hpp"
#include "ui/LoginDialog.hpp"
#include "ui/MainWindow.hpp"
#include "ui/components/FormDialog.hpp"
#include <QApplication>
#include <QFile>
#include <QMessageBox>
#include <QPainter>
#include <QSplashScreen>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTimer>
#include <memory>
int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName("SecuredGlobe");
    QApplication::setOrganizationName("SecuredGlobe");
    try {
        Q_INIT_RESOURCE(resources);
        QFile style(":/assets/styles/app.qss");
        if (!style.open(QIODevice::ReadOnly))
            throw std::runtime_error("Cannot load application style");
        app.setStyleSheet(QString::fromUtf8(style.readAll()));
        const bool smoke = app.arguments().contains("--smoke-test");
        QTemporaryDir temporary;
        QString path = smoke ? temporary.filePath("smoke.sqlite")
                             : QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) +
                                   "/banking.sqlite";
        int i = app.arguments().indexOf("--database");
        if (i >= 0) {
            if (i + 1 >= app.arguments().size())
                throw std::runtime_error("--database requires a file path");
            path = app.arguments()[i + 1];
        }
        QPixmap art(600, 340);
        art.fill(QColor("#101f30"));
        {
            QPainter p(&art);
            p.setPen(Qt::white);
            p.setFont(QFont("Arial", 27, QFont::Bold));
            p.drawText(art.rect().adjusted(30, 60, -30, -100), Qt::AlignCenter, "◈ SecuredGlobe");
            p.setFont(QFont("Arial", 12));
            p.setPen(QColor("#8cc9ba"));
            p.drawText(art.rect().adjusted(30, 180, -30, -20), Qt::AlignCenter,
                       "Advanced Banking Management\nInitializing secure local "
                       "workspace…\nFictional demonstration data");
        }
        QSplashScreen splash(art);
        splash.show();
        app.processEvents();
        Database database(path);
        BankRepository repository(database);
        SeedService::initialize(repository);
        SessionManager session;
        AuthService auth(repository, session);
        BankingService bank(repository, session);
        splash.close();
        if (smoke) {
            LoginDialog login(auth);
            FormDialog reset("Password assistance", "Administrator-assisted recovery");
            reset.text("username", "Username");
            reset.submit("Request", [&auth](const QVariantMap &data) {
                auth.requestReset(data["username"].toString());
            });
            auth.login("admin", "DemoBank!2026");
            auth.changePassword("DemoBank!2026", "VerifiedSmoke!2026");
            MainWindow window(bank, auth, session);
            return window.smokeAllScreens() ? 0 : 1;
        }
        for (;;) {
            LoginDialog login(auth);
            if (login.exec() != QDialog::Accepted)
                break;
            while (session.user().mustChange) {
                FormDialog change("Change temporary password",
                                  "Choose a unique password before entering the workspace.");
                change.text("current", "Current password", {}, true);
                change.text("next", "New password", {}, true);
                change.text("confirm", "Confirm new password", {}, true);
                change.submit("Set password", [&auth](const QVariantMap &v) {
                    if (v["next"] != v["confirm"])
                        throw std::runtime_error("Passwords do not match");
                    auth.changePassword(v["current"].toString(), v["next"].toString());
                });
                if (change.exec() != QDialog::Accepted) {
                    auth.logout();
                    break;
                }
            }
            if (!session.active())
                continue;
            MainWindow window(bank, auth, session);
            QObject::connect(&window, &MainWindow::logoutRequested, &window, &QMainWindow::close);
            window.show();
            app.exec();
            if (session.active())
                auth.logout();
        }
        return 0;
    } catch (const std::exception &e) {
        if (app.arguments().contains("--smoke-test")) {
            qCritical("%s", e.what());
            return 1;
        }
        QMessageBox::critical(nullptr, "SecuredGlobe startup error", QString::fromUtf8(e.what()));
        return 1;
    }
}
