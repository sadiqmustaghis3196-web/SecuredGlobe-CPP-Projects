#include "FormDialog.hpp"
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
FormDialog::FormDialog(const QString &title, const QString &description, QWidget *p) : QDialog(p) {
    setWindowTitle(title);
    setMinimumWidth(470);
    auto layout = new QVBoxLayout(this);
    auto heading = new QLabel(title);
    heading->setObjectName("pageTitle");
    layout->addWidget(heading);
    auto sub = new QLabel(description);
    sub->setWordWrap(true);
    sub->setObjectName("muted");
    layout->addWidget(sub);
    form_ = new QFormLayout;
    form_->setSpacing(14);
    layout->addLayout(form_);
    error_ = new QLabel;
    error_->setWordWrap(true);
    error_->setObjectName("error");
    layout->addWidget(error_);
}
QLineEdit *FormDialog::text(const QString &key, const QString &label, const QString &value,
                            bool password) {
    auto w = new QLineEdit(value);
    w->setMaxLength(password ? 128 : 250);
    if (password)
        w->setEchoMode(QLineEdit::Password);
    w->setAccessibleName(label);
    fields_[key] = w;
    form_->addRow(label, w);
    return w;
}
QComboBox *FormDialog::choices(const QString &key, const QString &label,
                               const QVector<QPair<qint64, QString>> &values, qint64 selected) {
    auto w = new QComboBox;
    for (const auto &v : values)
        w->addItem(v.second, v.first);
    auto i = w->findData(selected);
    if (i >= 0)
        w->setCurrentIndex(i);
    w->setAccessibleName(label);
    fields_[key] = w;
    form_->addRow(label, w);
    return w;
}
QComboBox *FormDialog::choices(const QString &key, const QString &label, const QStringList &values,
                               const QString &selected) {
    auto w = new QComboBox;
    w->addItems(values);
    if (!selected.isEmpty())
        w->setCurrentText(selected);
    w->setAccessibleName(label);
    fields_[key] = w;
    form_->addRow(label, w);
    return w;
}
void FormDialog::note(const QString &text) {
    auto label = new QLabel(text);
    label->setWordWrap(true);
    form_->addRow(label);
}
void FormDialog::submit(const QString &label, std::function<void(const QVariantMap &)> handler) {
    auto buttons = new QDialogButtonBox(QDialogButtonBox::Cancel);
    auto save = buttons->addButton(label, QDialogButtonBox::AcceptRole);
    save->setObjectName("primary");
    layout()->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(save, &QPushButton::clicked, this, [this, handler] {
        try {
            QVariantMap data;
            for (auto it = fields_.cbegin(); it != fields_.cend(); ++it) {
                if (auto input = qobject_cast<QLineEdit *>(it.value()))
                    data[it.key()] = input->text();
                else if (auto combo = qobject_cast<QComboBox *>(it.value()))
                    data[it.key()] = combo->currentData().isValid()
                                         ? combo->currentData()
                                         : QVariant(combo->currentText());
            }
            handler(data);
            accept();
        } catch (const std::exception &e) {
            error_->setText(QString::fromUtf8(e.what()));
        }
    });
}
