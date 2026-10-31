#include "PasswordHasher.hpp"
#include <QCryptographicHash>
#include <QPasswordDigestor>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <stdexcept>
QByteArray PasswordHasher::salt() {
    QByteArray out(24, '\0');
    for (int i = 0; i < 24; i += 4) {
        quint32 n = QRandomGenerator::system()->generate();
        for (int j = 0; j < 4; ++j)
            out[i + j] = static_cast<char>((n >> (8 * j)) & 255);
    }
    return out;
}
QByteArray PasswordHasher::hash(const QString &password, const QByteArray &salt, int iterations) {
    if (iterations < 100000)
        throw std::runtime_error("Invalid password hashing configuration");
    return QPasswordDigestor::deriveKeyPbkdf2(QCryptographicHash::Sha256, password.toUtf8(), salt,
                                              iterations, 32);
}
bool PasswordHasher::verify(const QString &password, const QByteArray &salt,
                            const QByteArray &expected, int iterations) {
    auto actual = hash(password, salt, iterations);
    if (actual.size() != expected.size())
        return false;
    unsigned char diff = 0;
    for (qsizetype i = 0; i < actual.size(); ++i)
        diff |= static_cast<unsigned char>(actual[i] ^ expected[i]);
    return diff == 0;
}
void PasswordHasher::validate(const QString &p) {
    if (p.size() < 12 || p.size() > 128 || !p.contains(QRegularExpression("[A-Z]")) ||
        !p.contains(QRegularExpression("[a-z]")) || !p.contains(QRegularExpression("[0-9]")) ||
        !p.contains(QRegularExpression("[^A-Za-z0-9]")))
        throw std::runtime_error(
            "Use 12–128 characters with uppercase, lowercase, a number, and a symbol");
}
