#pragma once

#include <QString>

namespace navi {

class AppLauncher {
public:
  bool launch(const QString &appPath) const;
};

} // namespace navi
