#include "platform/CrystalBackdrop.h"

namespace locus {

// Platforms without a native backdrop port yet: the widget's painted tint
// carries the disc (Windows Acrylic and KDE blur plug in behind this API).
Backdrop installCrystalBackdrop(QWidget *, const QRectF &, bool) {
  return Backdrop::Tint;
}

} // namespace locus
