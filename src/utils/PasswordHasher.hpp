#pragma once
#include <QByteArray>
#include <QString>
class PasswordHasher final {
  public:
    static constexpr int Iterations = 600000;
    static QByteArray salt();
    static QByteArray hash(const QString &password, const QByteArray &salt,
                           int iterations = Iterations);
    static bool verify(const QString &password, const QByteArray &salt, const QByteArray &expected,
                       int iterations);
    static void validate(const QString &password);
};
