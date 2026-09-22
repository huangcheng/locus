#pragma once

class QWidget;

namespace locus {

/// Keeps the overlay window interactive while Locus is not the active app:
/// acceptsMouseMovedEvents on the NSWindow plus a global mouse-moved monitor
/// that forwards hovers inside the overlay as synthesized Qt mouse events.
void macMakeOverlayLiveWhenInactive(QWidget *overlay, QWidget *content);

} // namespace locus
