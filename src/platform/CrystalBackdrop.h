#pragma once

class QRectF;
class QWidget;

namespace locus {

/// What material the platform actually installed behind the overlay. The
/// widget paints identical geometry and motion on every OS; it only adapts
/// painted opacity so the UI stays legible over the installed material.
enum class Backdrop {
  Tint,    // no native material — the painted fallback carries the disc
  Frosted, // native blur (macOS vibrancy, Win11 Acrylic, KDE blur)
  Glass,   // real refraction glass (macOS 26+ NSGlassEffectView)
};

/// Installs (or updates) the native backdrop behind the overlay and returns
/// what is in place. `discRect` is in content-widget points; an invalid rect
/// removes the backdrop (styles that don't use it) and returns Tint.
Backdrop installCrystalBackdrop(QWidget *overlay, const QRectF &discRect,
                                bool dark);

} // namespace locus
