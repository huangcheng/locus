#pragma once

class QRectF;
class QWidget;

namespace locus {

/// Keeps the overlay window interactive while Locus is not the active app:
/// acceptsMouseMovedEvents on the NSWindow plus a global mouse-moved monitor
/// that forwards hovers inside the overlay as synthesized Qt mouse events.
void macMakeOverlayLiveWhenInactive(QWidget *overlay, QWidget *content);

/// Installs (or updates) a real frosted-glass backdrop behind the overlay:
/// an NSVisualEffectView masked to the disc circle, so the launcher glass is
/// crystalline over any wallpaper. `discRect` is in content-widget points;
/// an invalid rect removes the backdrop (styles that don't use it).
void macInstallCrystalBackdrop(QWidget *overlay, const QRectF &discRect,
                               bool dark);

} // namespace locus
