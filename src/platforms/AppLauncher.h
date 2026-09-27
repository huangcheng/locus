#pragma once

#include <QString>

namespace locus {

class AppLauncher {
public:
  bool launch(const QString &appPath) const;

#ifdef Q_OS_WIN
  /// ShellExecute "runas" — triggers the UAC prompt. Only meaningful for
  /// executables.
  bool launchElevated(const QString &appPath) const;
#endif

  /// Open the file manager with `path` selected.
  void revealInFileManager(const QString &path) const;
};

} // namespace locus
