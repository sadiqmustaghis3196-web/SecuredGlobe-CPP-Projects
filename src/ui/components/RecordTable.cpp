#include "RecordTable.hpp"
#include "MoneyTableItem.hpp"
#include "utils/Money.hpp"
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPrintDialog>
#include <QPrinter>
#include <QPushButton>
#include <QTableWidget>
#include <QTextDocument>
#include <QVBoxLayout>
#include <algorithm>
#include <stdexcept>
static bool isMoney(const QString &h) {
    return QStringList{"Balance",
                       "Principal",
                       "Amount",
                       "Outstanding",
                       "Payment",
                       "Principal portion",
                       "Interest portion",
                       "Remaining principal",
                       "Paid toward schedule",
                       "Inflow",
                       "Outflow"}
        .contains(h);
}
RecordTable::RecordTable(TableData data, QWidget *p) : QWidget(p), data_(std::move(data)) {
    auto v = new QVBoxLayout(this);
    v->setContentsMargins(0, 0, 0, 0);
    auto bar = new QHBoxLayout;
    search_ = new QLineEdit;
    search_->setPlaceholderText("Search all columns…");
    search_->setAccessibleName("Search records");
    bar->addWidget(search_);
    auto csv = new QPushButton("Export CSV"), pdf = new QPushButton("Export PDF"),
         pr = new QPushButton("Print");
    bar->addWidget(csv);
    bar->addWidget(pdf);
    bar->addWidget(pr);
    v->addLayout(bar);
    table_ = new QTableWidget;
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->verticalHeader()->hide();
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->setAlternatingRowColors(true);
    v->addWidget(table_);
    auto bottom = new QHBoxLayout;
    count_ = new QLabel;
    bottom->addWidget(count_);
    bottom->addStretch();
    auto prev = new QPushButton("← Previous"), next = new QPushButton("Next →");
    bottom->addWidget(prev);
    bottom->addWidget(next);
    v->addLayout(bottom);
    connect(search_, &QLineEdit::textChanged, this, [this] {
        page_ = 0;
        populate();
    });
    connect(prev, &QPushButton::clicked, this, [this] {
        page_ = qMax(0, page_ - 1);
        populate();
    });
    connect(next, &QPushButton::clicked, this, [this] {
        if ((page_ + 1) * 20 < filtered().rows.size())
            ++page_;
        populate();
    });
    connect(csv, &QPushButton::clicked, this, &RecordTable::exportCsv);
    connect(pdf, &QPushButton::clicked, this, &RecordTable::exportPdf);
    connect(pr, &QPushButton::clicked, this, &RecordTable::print);
    connect(table_, &QTableWidget::cellDoubleClicked, this,
            [this](int, int) { emit activated(selectedId()); });
    populate();
}
TableData RecordTable::filtered() const {
    TableData out;
    out.headers = data_.headers;
    QString query = search_->text().trimmed();
    for (const auto &row : data_.rows) {
        QStringList parts;
        for (const auto &c : row)
            parts << c.toString();
        for (int i = 0; i < row.size(); ++i)
            if (isMoney(data_.headers[i]))
                parts << Money::format(row[i].toLongLong());
        if (query.isEmpty() || parts.join(' ').contains(query, Qt::CaseInsensitive))
            out.rows << row;
    }
    return out;
}
void RecordTable::populate() {
    auto data = filtered();
    table_->setSortingEnabled(false);
    table_->clear();
    table_->setColumnCount(data.headers.size());
    table_->setHorizontalHeaderLabels(data.headers);
    int start = page_ * 20, end = qMin(start + 20, static_cast<int>(data.rows.size()));
    table_->setRowCount(qMax(0, end - start));
    for (int i = start; i < end; ++i)
        for (int j = 0; j < data.headers.size(); ++j) {
            auto value = data.rows[i][j];
            QTableWidgetItem *item =
                isMoney(data.headers[j])
                    ? static_cast<QTableWidgetItem *>(new MoneyTableItem(value.toLongLong()))
                    : new QTableWidgetItem;
            if (!isMoney(data.headers[j]))
                item->setData(Qt::DisplayRole, value);
            item->setData(Qt::UserRole, data.rows[i].value(0));
            table_->setItem(i - start, j, item);
        }
    table_->resizeColumnsToContents();
    table_->setSortingEnabled(true);
    if (table_->rowCount())
        table_->selectRow(0);
    count_->setText(QString("%1–%2 of %3 records · page %4")
                        .arg(data.rows.isEmpty() ? 0 : start + 1)
                        .arg(end)
                        .arg(data.rows.size())
                        .arg(page_ + 1));
}
qint64 RecordTable::selectedId() const {
    auto row = table_->currentRow();
    if (row < 0 || !table_->item(row, 0))
        throw std::runtime_error("Select a record first");
    return table_->item(row, 0)->data(Qt::UserRole).toLongLong();
}
QVariantList RecordTable::selectedRow() const {
    auto id = selectedId();
    for (const auto &row : data_.rows)
        if (row[0].toLongLong() == id)
            return row;
    throw std::runtime_error("Selected record not found");
}
void RecordTable::exportCsv() {
    auto path =
        QFileDialog::getSaveFileName(this, "Export visible filtered records", {}, "CSV (*.csv)");
    if (path.isEmpty())
        return;
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) {
        QMessageBox::critical(this, "Export failed", f.errorString());
        return;
    }
    auto data = filtered();
    QString output;
    auto append = [&output](const QVariantList &values) {
        QStringList row;
        for (const auto &v : values) {
            QString s = v.toString();
            if (!s.isEmpty() && QString("=+-@\t\r").contains(s[0]))
                s.prepend('\'');
            s.replace('"', "\"\"");
            row << "\"" + s + "\"";
        }
        output += row.join(',') + "\r\n";
    };
    QVariantList headers;
    for (const auto &h : data.headers)
        headers << h;
    append(headers);
    for (auto row : data.rows) {
        for (int i = 0; i < row.size(); ++i)
            if (isMoney(data.headers[i]))
                row[i] = Money::format(row[i].toLongLong());
        append(row);
    }
    auto bytes = output.toUtf8();
    if (f.write(bytes) != bytes.size())
        QMessageBox::critical(this, "Export failed", f.errorString());
}
QString RecordTable::html() const {
    auto data = filtered();
    QString html = "<h1>SecuredGlobe</h1><p>Fictional demonstration data · Filtered "
                   "report</p><table border='1' cellspacing='0' cellpadding='5'><tr>";
    for (const auto &h : data.headers)
        html += "<th>" + h.toHtmlEscaped() + "</th>";
    html += "</tr>";
    for (const auto &row : data.rows) {
        html += "<tr>";
        for (int i = 0; i < row.size(); ++i)
            html +=
                "<td>" +
                (isMoney(data.headers[i]) ? Money::format(row[i].toLongLong()) : row[i].toString())
                    .toHtmlEscaped() +
                "</td>";
        html += "</tr>";
    }
    return html + "</table>";
}
void RecordTable::exportPdf() {
    auto path = QFileDialog::getSaveFileName(this, "Export report", {}, "PDF (*.pdf)");
    if (path.isEmpty())
        return;
    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(path);
    printer.setPageOrientation(QPageLayout::Landscape);
    QTextDocument doc;
    doc.setHtml(html());
    doc.print(&printer);
    if (!QFileInfo::exists(path))
        QMessageBox::critical(this, "Export failed", "PDF file could not be created");
}
void RecordTable::print() {
    QPrinter printer(QPrinter::HighResolution);
    QPrintDialog dialog(&printer, this);
    if (dialog.exec() == QDialog::Accepted) {
        QTextDocument doc;
        doc.setHtml(html());
        doc.print(&printer);
    }
}
