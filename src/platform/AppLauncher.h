#pragma once

#include <QString>

namespace locus {

class AppLauncher {
public:
  bool launch(const QString &appPath) const;
};

} // namespace locus
