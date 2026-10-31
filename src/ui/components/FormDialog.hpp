#pragma once
#include <QDialog>
#include <QMap>
#include <QVariant>
#include <functional>
class QFormLayout;
class QLabel;
class QLineEdit;
class QComboBox;
class FormDialog final : public QDialog {
  public:
    FormDialog(const QString &title, const QString &description, QWidget *parent = nullptr);
    QLineEdit *text(const QString &key, const QString &label, const QString &value = {},
                    bool password = false);
    QComboBox *choices(const QString &key, const QString &label,
                       const QVector<QPair<qint64, QString>> &values, qint64 selected = 0);
    QComboBox *choices(const QString &key, const QString &label, const QStringList &values,
                       const QString &selected = {});
    void note(const QString &text);
    void submit(const QString &label, std::function<void(const QVariantMap &)> handler);

  private:
    QFormLayout *form_;
    QLabel *error_;
    QMap<QString, QWidget *> fields_;
};
