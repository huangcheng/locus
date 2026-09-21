#pragma once

#include "core/Types.h"

#include <QString>

namespace locus {

struct Pin {
  PinId id;
  QString appPath;
  QString label;
  QString iconKey;
};

} // namespace locus
