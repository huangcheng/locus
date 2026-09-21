#pragma once

#include "core/Types.h"

#include <QString>

namespace navi {

struct Pin {
  PinId id;
  QString appPath;
  QString label;
  QString iconKey;
};

} // namespace navi
