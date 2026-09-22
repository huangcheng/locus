#include "core/Localization.h"
#include "core/PinStore.h"
#include "core/Prefs.h"
#include "core/SessionController.h"
#include "layout/CellularLayoutStrategy.h"
#include "layout/FanLayoutStrategy.h"
#include "layout/OrbitalLayoutStrategy.h"
#include "platform/AppLauncher.h"
#include "platform/CrystalBackdrop.h"
#include "platform/HotkeyManager.h"
#include "platform/IconProvider.h"
#include "platform/MacActivation.h"
#include "platform/MacOverlay.h"
#include "platform/TrayController.h"
#include "ui/CellularGlassView.h"
#include "ui/FanGlassView.h"
#include "ui/OrbitalGlassView.h"
#include "ui/OverlayWindow.h"
#include "ui/PrefsWindow.h"

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

QVector<locus::Pin> seedMacApps() {
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
  QVector<locus::Pin> pins;
  for (const QString &path : candidates) {
    if (!QFileInfo::exists(path))
      continue;
    locus::Pin pin;
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
  // resources.qrc is compiled into the static locus_core lib; without this the
  // linker drops it and QPixmap(":/...") loads fail silently.
  Q_INIT_RESOURCE(resources);
  QCoreApplication::setOrganizationName(QStringLiteral("Locus"));
  QCoreApplication::setApplicationName(QStringLiteral("Locus"));
  app.setQuitOnLastWindowClosed(false);

  // Single instance: a second launch pings the running instance (which then
  // shows the widget) and exits immediately. `--prefs` opens Settings instead.
  const QString instanceServer = QStringLiteral("im.cheng.locus.single");
  const bool wantsPrefs =
      QStringList::fromVector(app.arguments()).contains(QLatin1String("--prefs"));
  {
    QLocalSocket probe;
    probe.connectToServer(instanceServer, QIODevice::WriteOnly);
    if (probe.waitForConnected(200)) {
      probe.write(wantsPrefs ? "prefs" : "show");
      probe.flush();
      probe.waitForBytesWritten(200);
      return 0;
    }
  }

  locus::macActivateApplication();

  QSettings settings;
  locus::Prefs prefs(&settings);
  prefs.load();
  locus::installLocusTranslator(prefs.language());
  locus::PinStore pinStore(&settings);
  pinStore.load();
  if (pinStore.pins().isEmpty())
    pinStore.setPins(seedMacApps());

  locus::SessionController session;
  locus::IconProvider icons;
  locus::AppLauncher launcher;

  // Resolve System against the real OS palette so dark glass never floats
  // over a light desktop (and vice versa).
  auto resolveAppearance = [&] {
    if (prefs.appearance() == locus::Appearance::System)
      return QGuiApplication::styleHints()->colorScheme() ==
                     Qt::ColorScheme::Dark
                 ? locus::Appearance::Dark
                 : locus::Appearance::Light;
    return prefs.appearance();
  };

  locus::CellularLayoutStrategy cellularStrategy;
  locus::OrbitalLayoutStrategy orbitalStrategy;
  locus::FanLayoutStrategy fanStrategy;

  locus::CellularGlassView *cellularView = nullptr;
  locus::OrbitalGlassView *orbitalView = nullptr;
  locus::FanGlassView *fanView = nullptr;
  locus::MenuView *view = nullptr;

  // Placeholder content; applyStyle() swaps in the real view below.
  locus::OverlayWindow overlay(new QWidget);

  auto pickStrategy = [&]() -> locus::LayoutStrategy * {
    switch (prefs.styleId()) {
    case locus::StyleId::Cellular:
      return &cellularStrategy;
    case locus::StyleId::Fan:
      return &fanStrategy;
    case locus::StyleId::Orbital:
    case locus::StyleId::Pie:
      return &orbitalStrategy;
    }
    return &orbitalStrategy;
  };

  auto rebuild = [&] {
    const auto scene = pickStrategy()->build(pinStore.pins(), session.focusedId(),
                                             prefs.density());
    view->setScene(scene);
    overlay.resizeToContent();
    // Only while shown: a hidden overlay still carries its stale frame (and
    // possibly the wrong screen/scale), which is how the glass once landed
    // as a 2x giant on the wrong monitor. showMenuAt re-installs post-show.
    if (orbitalView && overlay.isVisible())
      orbitalView->setBackdrop(locus::installCrystalBackdrop(
          &overlay, orbitalView->discRect(),
          resolveAppearance() == locus::Appearance::Dark));
  };

  auto applyIcons = [&] {
    for (const auto &pin : pinStore.pins()) {
      const QIcon icon = icons.iconForPath(pin.appPath);
      if (cellularView)
        cellularView->setIcon(pin.id, icon);
      if (orbitalView)
        orbitalView->setIcon(pin.id, icon);
      if (fanView)
        fanView->setIcon(pin.id, icon);
    }
  };

  auto showMenuAt = [&](const QPoint &anchor) {
    session.open();
    if (session.focusedId().isEmpty() && !pinStore.pins().isEmpty())
      session.setFocus(pinStore.pins().first().id);
    rebuild();
    if (cellularView)
      cellularView->playOpenAnimation();
    overlay.showAt(anchor);
    // The window is now at its final position/screen/size — compute the
    // glass against this frame, never the stale pre-show one.
    if (orbitalView)
      orbitalView->setBackdrop(locus::installCrystalBackdrop(
          &overlay, orbitalView->discRect(),
          resolveAppearance() == locus::Appearance::Dark));
    locus::macActivateApplication();
  };

  // Tray summon centers on the primary screen (cursor-anchored summon is the
  // global hotkey's job), falling back to the cursor position if none exists.
  auto showMenu = [&] {
    const QPoint anchor =
        QGuiApplication::primaryScreen()
            ? QGuiApplication::primaryScreen()->availableGeometry().center()
            : QCursor::pos();
    showMenuAt(anchor);
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
  auto createView = [&] {
    locus::CellularGlassView *cv = nullptr;
    locus::OrbitalGlassView *ov = nullptr;
    locus::FanGlassView *fv = nullptr;
    cellularView = nullptr;
    orbitalView = nullptr;
    fanView = nullptr;
    if (prefs.styleId() == locus::StyleId::Cellular) {
      cv = new locus::CellularGlassView;
      cv->setAppearance(resolveAppearance());
      cv->setIconSize(prefs.density().iconSize);
      cellularView = cv;
      view = cv;
      wireView(cv);
    } else if (prefs.styleId() == locus::StyleId::Fan) {
      fv = new locus::FanGlassView;
      fv->setAppearance(resolveAppearance());
      fv->setIconSize(prefs.density().iconSize);
      fanView = fv;
      view = fv;
      wireView(fv);
    } else {
      ov = new locus::OrbitalGlassView;
      ov->setAppearance(resolveAppearance());
      ov->setIconSize(prefs.density().iconSize);
      orbitalView = ov;
      view = ov;
      wireView(ov);
    }
    applyIcons();
  };

  // (Re)create the launcher view — at startup and when the menu style
  // changes in Settings.
  auto applyStyle = [&] {
    createView();
    overlay.setContent(view->widget());
    locus::macMakeOverlayLiveWhenInactive(&overlay, view->widget());
    if (!orbitalView) // only Orbit uses the crystal disc backdrop
      locus::installCrystalBackdrop(&overlay, QRectF(), false);
    rebuild();
  };
  applyStyle();

  // Launcher semantics: focusing another app dismisses the widget.
  QObject::connect(&app, &QApplication::applicationStateChanged, &app,
                   [&](Qt::ApplicationState state) {
                     if (state != Qt::ApplicationActive && session.isOpen())
                       hideMenu();
                   });

  locus::TrayController tray;
  locus::PrefsWindow prefsWindow(&prefs, &pinStore, &icons);
  prefsWindow.setResolvedAppearance(resolveAppearance());

  auto applyAppearance = [&] {
    const locus::Appearance resolved = resolveAppearance();
    if (cellularView)
      cellularView->setAppearance(resolved);
    if (fanView)
      fanView->setAppearance(resolved);
    if (orbitalView) {
      orbitalView->setAppearance(resolved);
      orbitalView->setBackdrop(locus::installCrystalBackdrop(
          &overlay, orbitalView->discRect(),
          resolved == locus::Appearance::Dark));
    }
    prefsWindow.setResolvedAppearance(resolved);
  };
  QObject::connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged,
                   &app, applyAppearance);
  QObject::connect(&prefsWindow, &locus::PrefsWindow::appearanceChanged, &app,
                   applyAppearance);
  QObject::connect(&prefsWindow, &locus::PrefsWindow::styleChanged, &app,
                   applyStyle);
  QObject::connect(&prefsWindow, &locus::PrefsWindow::densityChanged, &app, [&] {
    if (cellularView)
      cellularView->setIconSize(prefs.density().iconSize);
    if (orbitalView)
      orbitalView->setIconSize(prefs.density().iconSize);
    if (fanView)
      fanView->setIconSize(prefs.density().iconSize);
    rebuild();
  });
  QObject::connect(&prefsWindow, &locus::PrefsWindow::pinsChanged, &app, [&] {
    applyIcons();
    rebuild();
  });
  QObject::connect(&prefsWindow, &locus::PrefsWindow::languageChanged, &app,
                   [&] {
                     locus::installLocusTranslator(prefs.language());
                     prefsWindow.retranslateUi();
                     tray.retranslate();
                   });
  QObject::connect(&tray, &locus::TrayController::showRequested, &app, showMenu);
  QObject::connect(&tray, &locus::TrayController::quitRequested, &app,
                   &QApplication::quit);

  // Global hotkey: toggles the widget from any app; re-registers live when
  // the user records a new combo in Settings.
  locus::HotkeyManager hotkeyManager;
  hotkeyManager.setHotkey(prefs.hotkey());
  QObject::connect(&hotkeyManager, &locus::HotkeyManager::triggered, &app,
                   [&] {
                     if (session.isOpen())
                       hideMenu();
                     else
                       showMenuAt(QCursor::pos());
                   });
  QObject::connect(&prefsWindow, &locus::PrefsWindow::hotkeyChanged, &app,
                   [&] { hotkeyManager.setHotkey(prefs.hotkey()); });
  auto showPrefs = [&] {
    if (session.isOpen())
      hideMenu();
    prefsWindow.refreshFromModel();
    prefsWindow.show();
    prefsWindow.raise();
    locus::macActivateApplication();
  };
  QObject::connect(&tray, &locus::TrayController::prefsRequested, &app,
                   showPrefs);

  QLocalServer singleInstanceGuard;
  QLocalServer::removeServer(instanceServer); // clear a stale socket after crashes
  singleInstanceGuard.listen(instanceServer);
  QObject::connect(&singleInstanceGuard, &QLocalServer::newConnection, &app,
                   [&] {
                     while (QLocalSocket *sock =
                                singleInstanceGuard.nextPendingConnection()) {
                       sock->waitForReadyRead(100);
                       const QByteArray msg = sock->readAll().trimmed();
                       sock->deleteLater();
                       if (msg == "prefs")
                         showPrefs();
                       else
                         showMenu(); // the ping itself is the signal
                     }
                   });

  if (wantsPrefs)
    showPrefs();
  else
    showMenu();
  return app.exec();
}
