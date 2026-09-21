#include "platform/TrayController.h"

#include <QAction>
#include <QApplication>
#include <QIcon>
#include <QMenu>
#include <QMessageBox>
#include <QPixmap>
#include <QStyle>

namespace navi {

namespace {

QIcon trayIcon() {
  QIcon icon;
  QPixmap px1(QStringLiteral(":/resources/trayTemplate.png"));
  QPixmap px2(QStringLiteral(":/resources/trayTemplate@2x.png"));
  if (!px1.isNull())
    icon.addPixmap(px1);
  if (!px2.isNull())
    icon.addPixmap(px2);
  if (icon.isNull())
    icon = QApplication::style()->standardIcon(QStyle::SP_ComputerIcon);
  // Required for macOS menu bar tinting.
  icon.setIsMask(true);
  return icon;
}

} // namespace

TrayController::TrayController(QObject *parent) : QObject(parent), tray_(this) {
  menu_ = new QMenu;
  auto *showAction = menu_->addAction(tr("Show Navi"));
  auto *prefsAction = menu_->addAction(tr("Preferences…"));
  menu_->addSeparator();
  auto *quitAction = menu_->addAction(tr("Quit"));

  connect(showAction, &QAction::triggered, this, &TrayController::showRequested);
  connect(prefsAction, &QAction::triggered, this, &TrayController::prefsRequested);
  connect(quitAction, &QAction::triggered, this, &TrayController::quitRequested);
  connect(&tray_, &QSystemTrayIcon::activated, this,
          [this](QSystemTrayIcon::ActivationReason reason) {
            if (reason == QSystemTrayIcon::Trigger ||
                reason == QSystemTrayIcon::DoubleClick)
              emit showRequested();
          });

  tray_.setContextMenu(menu_);
  tray_.setIcon(trayIcon());
  tray_.setToolTip(tr("Navi"));

  if (!QSystemTrayIcon::isSystemTrayAvailable()) {
    QMessageBox::warning(
        nullptr, tr("Navi"),
        tr("System tray is unavailable. Use the Dock icon or reopen the app."));
    return;
  }

  tray_.show();
  // Nudge macOS status item registration for non-activated launches.
  tray_.hide();
  tray_.show();
}

} // namespace navi
