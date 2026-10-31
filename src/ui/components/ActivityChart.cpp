#include "ActivityChart.hpp"
#include "utils/Money.hpp"
#include <QPainter>
ActivityChart::ActivityChart(TableData data, QWidget *p) : QWidget(p), data_(std::move(data)) {
    setMinimumHeight(240);
    setAccessibleName("Monthly inflows and outflows chart");
}
void ActivityChart::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), Qt::white);
    p.setPen(QColor("#6c8192"));
    p.drawText(12, 20, "Monthly activity   ● Inflow / ● Outflow");
    if (data_.rows.isEmpty()) {
        p.drawText(rect(), Qt::AlignCenter, "No transaction activity yet");
        return;
    }
    qint64 max = 1;
    for (const auto &row : data_.rows)
        max = qMax(max, qMax(row[1].toLongLong(), row[2].toLongLong()));
    QRectF area(85, 40, width() - 110, height() - 85);
    for (int i = 0; i <= 4; ++i) {
        double y = area.top() + area.height() * i / 4;
        p.setPen(QColor("#e6ecee"));
        p.drawLine(QPointF(area.left(), y), QPointF(area.right(), y));
        p.setPen(QColor("#8b9ca6"));
        p.drawText(QRectF(0, y - 9, 78, 20), Qt::AlignRight, Money::format(max * (4 - i) / 4));
    }
    double step = area.width() / data_.rows.size();
    for (int i = 0; i < data_.rows.size(); ++i) {
        const auto &row = data_.rows[i];
        for (int j = 1; j <= 2; ++j) {
            double h =
                area.height() * static_cast<double>(row[j].toLongLong()) / static_cast<double>(max);
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(j == 1 ? "#218575" : "#b3c6df"));
            p.drawRoundedRect(QRectF(area.left() + i * step + (j == 1 ? step * .12 : step * .52),
                                     area.bottom() - h, step * .3, h),
                              3, 3);
        }
        p.setPen(QColor("#7d919f"));
        p.drawText(QRectF(area.left() + i * step, area.bottom() + 8, step, 22), Qt::AlignCenter,
                   row[0].toString());
    }
}
