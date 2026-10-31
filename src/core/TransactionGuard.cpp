#include "TransactionGuard.hpp"
#include <QSqlError>
#include <stdexcept>
TransactionGuard::TransactionGuard(QSqlDatabase db) : db_(db) {
    if (!db_.transaction())
        throw std::runtime_error(db_.lastError().text().toStdString());
}
TransactionGuard::~TransactionGuard() {
    if (!committed_)
        db_.rollback();
}
void TransactionGuard::commit() {
    if (!db_.commit())
        throw std::runtime_error(db_.lastError().text().toStdString());
    committed_ = true;
}
