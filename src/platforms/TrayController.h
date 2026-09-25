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

  /// Tray balloon announcing an update found by the background check.
  void showUpdateAvailable(const QString &version);

signals:
  void showRequested();
  void prefsRequested();
  void quitRequested();
  void updateCheckRequested();

private:
  QSystemTrayIcon tray_;
  QMenu *menu_ = nullptr;
  QAction *prefsAction_ = nullptr;
  QAction *updateAction_ = nullptr;
  QAction *quitAction_ = nullptr;
};

} // namespace locus
