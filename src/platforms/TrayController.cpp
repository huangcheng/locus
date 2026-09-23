#include "platforms/TrayController.h"

#include <QAction>
#include <QApplication>
#include <QCursor>
#include <QIcon>
#include <QMenu>
#include <QMessageBox>
#include <QPixmap>
#include <QStyle>

namespace locus {

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
  prefsAction_ = menu_->addAction(tr("Preferences…"));
  menu_->addSeparator();
  quitAction_ = menu_->addAction(tr("Quit"));

  connect(prefsAction_, &QAction::triggered, this, &TrayController::prefsRequested);
  connect(quitAction_, &QAction::triggered, this, &TrayController::quitRequested);
  // No setContextMenu: an attached NSMenu intercepts ALL clicks on macOS, so
  // left-click could never reach us. Menu pops up manually on right-click.
  connect(&tray_, &QSystemTrayIcon::activated, this,
          [this](QSystemTrayIcon::ActivationReason reason) {
            if (reason == QSystemTrayIcon::Trigger ||
                reason == QSystemTrayIcon::DoubleClick)
              emit showRequested();
            else if (reason == QSystemTrayIcon::Context)
              menu_->popup(QCursor::pos());
          });

  tray_.setIcon(trayIcon());
  tray_.setToolTip(tr("Locus"));

  if (!QSystemTrayIcon::isSystemTrayAvailable()) {
    QMessageBox::warning(
        nullptr, tr("Locus"),
        tr("System tray is unavailable. Use the Dock icon or reopen the app."));
    return;
  }

  tray_.show();
  // Nudge macOS status item registration for non-activated launches.
  tray_.hide();
  tray_.show();
}

void TrayController::retranslate() {
  prefsAction_->setText(tr("Preferences…"));
  quitAction_->setText(tr("Quit"));
  tray_.setToolTip(tr("Locus"));
}

} // namespace locus
