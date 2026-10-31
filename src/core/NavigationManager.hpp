#pragma once
#include <QString>
#include <QVector>
struct ScreenInfo {
    int id;
    QString title;
};
class NavigationManager final {
  public:
    static const QVector<ScreenInfo> &screens();
    static QString title(int id);
    void visit(int id);
    int current() const;

  private:
    int current_ = 4;
};
