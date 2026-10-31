#pragma once
#include <QSqlDatabase>
#include <QString>
class Database final {
  public:
    explicit Database(const QString &path);
    ~Database();
    Database(const Database &) = delete;
    Database &operator=(const Database &) = delete;
    QSqlDatabase connection() const;
    const QString &path() const;

  private:
    QString name_, path_;
    QSqlDatabase db_;
    void migrate();
};
