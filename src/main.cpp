#include "core/PinStore.h"
#include "core/Prefs.h"
#include "core/SessionController.h"
#include "layout/CellularLayoutStrategy.h"
#include "layout/OrbitalLayoutStrategy.h"
#include "platform/AppLauncher.h"
#include "platform/IconProvider.h"
#include "platform/MacActivation.h"
#include "platform/MacOverlay.h"
#include "platform/TrayController.h"
#include "ui/CellularGlassView.h"
#include "ui/OrbitalGlassView.h"
#include "ui/OverlayWindow.h"

#include <QApplication>
#include <QCursor>
#include <QFileInfo>
#include <QGuiApplication>
#include <QLocalServer>
#include <QLocalSocket>
#include <QScreen>
#include <QSettings>
#include <QStyleHints>
#include <QTimer>
#include <QUuid>

#include <type_traits>

namespace {

QVector<navi::Pin> seedMacApps() {
  const QStringList candidates = {
      QStringLiteral("/Applications/Safari.app"),
      QStringLiteral("/System/Applications/Utilities/Terminal.app"),
      QStringLiteral("/Applications/Visual Studio Code.app"),
      QStringLiteral("/Applications/Slack.app"),
      QStringLiteral("/Applications/Figma.app"),
      QStringLiteral("/System/Applications/Music.app"),
      QStringLiteral("/System/Applications/Finder.app"),
      QStringLiteral("/System/Applications/Notes.app"),
      QStringLiteral("/Applications/Google Chrome.app"),
      QStringLiteral("/Applications/Spotify.app"),
      QStringLiteral("/Applications/Notion.app"),
      QStringLiteral("/Applications/Discord.app"),
  };
  QVector<navi::Pin> pins;
  for (const QString &path : candidates) {
    if (!QFileInfo::exists(path))
      continue;
    navi::Pin pin;
    pin.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    pin.appPath = path;
    pin.label = QFileInfo(path).completeBaseName();
    pin.iconKey = path;
    pins.push_back(pin);
    if (pins.size() >= 12)
      break;
  }
  return pins;
}

} // namespace

int main(int argc, char *argv[]) {
  QApplication app(argc, argv);
  QCoreApplication::setOrganizationName(QStringLiteral("Navi"));
  QCoreApplication::setApplicationName(QStringLiteral("Navi"));
  app.setQuitOnLastWindowClosed(false);

  // Single instance: a second launch pings the running instance (which then
  // shows the widget) and exits immediately.
  const QString instanceServer = QStringLiteral("app.navi.launcher.single");
  {
    QLocalSocket probe;
    probe.connectToServer(instanceServer, QIODevice::WriteOnly);
    if (probe.waitForConnected(200)) {
      probe.write("show");
      probe.flush();
      probe.waitForBytesWritten(200);
      return 0;
    }
  }

  navi::macActivateApplication();

  QSettings settings;
  navi::Prefs prefs(&settings);
  prefs.load();
  navi::PinStore pinStore(&settings);
  pinStore.load();
  if (pinStore.pins().isEmpty())
    pinStore.setPins(seedMacApps());

  navi::SessionController session;
  navi::IconProvider icons;
  navi::AppLauncher launcher;

  const bool cellular = prefs.styleId() == navi::StyleId::Cellular;

  // Resolve System against the real OS palette so dark glass never floats
  // over a light desktop (and vice versa).
  auto resolveAppearance = [&] {
    if (prefs.appearance() == navi::Appearance::System)
      return QGuiApplication::styleHints()->colorScheme() ==
                     Qt::ColorScheme::Dark
                 ? navi::Appearance::Dark
                 : navi::Appearance::Light;
    return prefs.appearance();
  };

  navi::CellularLayoutStrategy cellularStrategy;
  navi::OrbitalLayoutStrategy orbitalStrategy;
  navi::LayoutStrategy *strategy =
      cellular ? static_cast<navi::LayoutStrategy *>(&cellularStrategy)
               : static_cast<navi::LayoutStrategy *>(&orbitalStrategy);

  navi::CellularGlassView *cellularView = nullptr;
  navi::OrbitalGlassView *orbitalView = nullptr;
  navi::MenuView *view = nullptr;
  if (cellular) {
    cellularView = new navi::CellularGlassView;
    cellularView->setAppearance(resolveAppearance());
    for (const auto &pin : pinStore.pins())
      cellularView->setIcon(pin.id, icons.iconForPath(pin.appPath));
    view = cellularView;
  } else {
    orbitalView = new navi::OrbitalGlassView;
    orbitalView->setAppearance(resolveAppearance());
    for (const auto &pin : pinStore.pins())
      orbitalView->setIcon(pin.id, icons.iconForPath(pin.appPath));
    view = orbitalView;
  }

  navi::OverlayWindow overlay(view->widget());
  navi::macMakeOverlayLiveWhenInactive(&overlay, view->widget());

  auto rebuild = [&] {
    const auto scene =
        strategy->build(pinStore.pins(), session.focusedId(), prefs.density());
    view->setScene(scene);
    overlay.resizeToContent();
  };

  auto showMenu = [&] {
    session.open();
    if (session.focusedId().isEmpty() && !pinStore.pins().isEmpty())
      session.setFocus(pinStore.pins().first().id);
    rebuild();
    if (cellularView)
      cellularView->playOpenAnimation();
    // Center on the primary screen (cursor-anchored summon arrives with the
    // global hotkey), falling back to the cursor position if none exists.
    const QPoint anchor =
        QGuiApplication::primaryScreen()
            ? QGuiApplication::primaryScreen()->availableGeometry().center()
            : QCursor::pos();
    overlay.showAt(anchor);
    navi::macActivateApplication();
  };

  auto hideMenu = [&] {
    session.close();
    overlay.hide();
  };

  auto wireView = [&](auto *v) {
    using V = std::remove_pointer_t<decltype(v)>;
    QObject::connect(v, &V::itemHovered, &session, [&](const QString &id) {
      if (id.isEmpty())
        return; // sticky selection: moving off cells keeps the current focus
      session.setFocus(id);
      rebuild();
    });
    QObject::connect(v, &V::itemActivated, &app, [&](const QString &id) {
      session.setFocus(id);
      const QString activated = session.activate();
      if (activated.isEmpty())
        return;
      for (const auto &pin : pinStore.pins()) {
        if (pin.id == activated) {
          launcher.launch(pin.appPath);
          break;
        }
      }
      // Let the click dip finish before the widget disappears; skip the hide
      // if the menu was re-summoned in the meantime (session open again).
      QTimer::singleShot(180, &overlay, [&] {
        if (!session.isOpen())
          overlay.hide();
      });
    });
    QObject::connect(v, &V::dismissRequested, &app, hideMenu);
  };
  if (cellularView)
    wireView(cellularView);
  else
    wireView(orbitalView);

  if (orbitalView) {
    QObject::connect(orbitalView, &navi::OrbitalGlassView::rotationDelta, &app,
                     [&](qreal delta) {
                       orbitalStrategy.setRotationRadians(
                           orbitalStrategy.rotationRadians() + delta);
                       rebuild();
                     });
  }

  // Launcher semantics: focusing another app dismisses the widget.
  QObject::connect(&app, &QApplication::applicationStateChanged, &app,
                   [&](Qt::ApplicationState state) {
                     if (state != Qt::ApplicationActive && session.isOpen())
                       hideMenu();
                   });

  navi::TrayController tray;
  QObject::connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged,
                   &app, [&] {
                     if (cellularView)
                       cellularView->setAppearance(resolveAppearance());
                     if (orbitalView)
                       orbitalView->setAppearance(resolveAppearance());
                   });
  QObject::connect(&tray, &navi::TrayController::showRequested, &app, showMenu);
  QObject::connect(&tray, &navi::TrayController::quitRequested, &app,
                   &QApplication::quit);
  QObject::connect(&tray, &navi::TrayController::prefsRequested, &app, [] {
    // PrefsDialog arrives in a later task.
  });

  QLocalServer singleInstanceGuard;
  QLocalServer::removeServer(instanceServer); // clear a stale socket after crashes
  singleInstanceGuard.listen(instanceServer);
  QObject::connect(&singleInstanceGuard, &QLocalServer::newConnection, &app,
                   [&] {
                     while (QLocalSocket *sock =
                                singleInstanceGuard.nextPendingConnection())
                       sock->deleteLater();
                     showMenu(); // the ping itself is the signal
                   });

  showMenu();
  return app.exec();
}
