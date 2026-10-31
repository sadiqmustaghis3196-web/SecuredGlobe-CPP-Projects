#pragma once
#include <QString>
class Money final {
  public:
    static qint64 parse(const QString &text);
    static QString format(qint64 cents);
    static void positive(qint64 cents);
    static qint64 add(qint64 balance, qint64 amount);
};
