#include "Database.hpp"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>
#include <stdexcept>
static void execute(QSqlDatabase db, const QString &sql) {
    QSqlQuery q(db);
    if (!q.exec(sql))
        throw std::runtime_error(q.lastError().text().toStdString());
}
Database::Database(const QString &path)
    : name_(QUuid::createUuid().toString(QUuid::WithoutBraces)), path_(path) {
    Q_INIT_RESOURCE(resources);
    if (path != ":memory:" && !QDir().mkpath(QFileInfo(path).absolutePath()))
        throw std::runtime_error("Cannot create database directory");
    db_ = QSqlDatabase::addDatabase("QSQLITE", name_);
    db_.setDatabaseName(path);
    db_.setConnectOptions("QSQLITE_BUSY_TIMEOUT=5000");
    if (!db_.open()) {
        const auto error = db_.lastError().text();
        db_ = QSqlDatabase();
        QSqlDatabase::removeDatabase(name_);
        throw std::runtime_error(error.toStdString());
    }
    try {
        execute(db_, "PRAGMA foreign_keys=ON");
        execute(db_, "PRAGMA journal_mode=DELETE");
        migrate();
    } catch (...) {
        db_.close();
        db_ = QSqlDatabase();
        QSqlDatabase::removeDatabase(name_);
        throw;
    }
}
Database::~Database() {
    db_.close();
    db_ = QSqlDatabase();
    QSqlDatabase::removeDatabase(name_);
}
QSqlDatabase Database::connection() const {
    return db_;
}
const QString &Database::path() const {
    return path_;
}
void Database::migrate() {
    execute(db_, "CREATE TABLE IF NOT EXISTS schema_migrations(version INTEGER PRIMARY KEY, "
                 "applied_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP)");
    QSqlQuery version(db_);
    if (!version.exec("SELECT COALESCE(MAX(version),0) FROM schema_migrations") || !version.next())
        throw std::runtime_error("Cannot read schema version");
    if (version.value(0).toInt() > 1)
        throw std::runtime_error("Database was created by a newer version");
    if (version.value(0).toInt() == 1)
        return;
    QFile f(":/database/migrations/001_initial.sql");
    if (!f.open(QIODevice::ReadOnly))
        throw std::runtime_error("Cannot read database migration");
    if (!db_.transaction())
        throw std::runtime_error("Cannot start migration");
    try {
        for (const auto &statement :
             QString::fromUtf8(f.readAll()).split(';', Qt::SkipEmptyParts)) {
            if (!statement.trimmed().isEmpty())
                execute(db_, statement.trimmed());
        }
        if (!db_.commit())
            throw std::runtime_error("Cannot commit migration");
    } catch (...) {
        db_.rollback();
        throw;
    }
}
