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

  /// Re-apply menu strings after the app language changes.
  void retranslate();

signals:
  void showRequested();
  void prefsRequested();
  void quitRequested();

private:
  QSystemTrayIcon tray_;
  QMenu *menu_ = nullptr;
  QAction *prefsAction_ = nullptr;
  QAction *quitAction_ = nullptr;
};

} // namespace locus
