#include "platforms/AppLauncher.h"

#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QUrl>

#ifdef Q_OS_WIN
#include <windows.h>
#include <shellapi.h>
#endif

namespace locus {

bool AppLauncher::launch(const QString &appPath) const {
  if (appPath.isEmpty())
    return false;
  return QDesktopServices::openUrl(QUrl::fromLocalFile(appPath));
}

#ifdef Q_OS_WIN
bool AppLauncher::launchElevated(const QString &appPath) const {
  if (appPath.isEmpty())
    return false;
  // ShellExecuteEx with the runas verb triggers UAC; QProcess can't elevate.
  const HINSTANCE result = ShellExecuteW(
      nullptr, L"runas",
      reinterpret_cast<LPCWSTR>(QDir::toNativeSeparators(appPath).utf16()),
      nullptr, nullptr, SW_SHOWNORMAL);
  return reinterpret_cast<qintptr>(result) > 32; // <= 32 is an error code
}
#endif

void AppLauncher::revealInFileManager(const QString &path) const {
#if defined(Q_OS_WIN)
  QProcess::startDetached(QStringLiteral("explorer.exe"),
                          {QStringLiteral("/select,"),
                           QDir::toNativeSeparators(path)});
#elif defined(Q_OS_MAC)
  QProcess::startDetached(QStringLiteral("open"),
                          {QStringLiteral("-R"), path});
#else
  // No portable "select file" on Linux; open the containing folder.
  QDesktopServices::openUrl(
      QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
#endif
}

} // namespace locus
