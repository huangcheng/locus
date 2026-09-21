#pragma once

#include <QObject>
#include <QSystemTrayIcon>

class QAction;
class QMenu;

namespace locus {

class TrayController : public QObject {
  Q_OBJECT
public:
  explicit TrayController(QObject *parent = nullptr);

signals:
  void showRequested();
  void prefsRequested();
  void quitRequested();

private:
  QSystemTrayIcon tray_;
  QMenu *menu_ = nullptr;
};

} // namespace locus
