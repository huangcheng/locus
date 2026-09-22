#pragma once

class QWidget;

namespace locus {

/// Make the process a regular foreground app and activate it (macOS).
void macActivateApplication();

/// Restyle a window as a seamless settings window: hidden title, transparent
/// title bar, content extending under the traffic lights, background-draggable.
void macStyleSettingsWindow(QWidget *window);

/// True when the user enabled macOS "Reduce Motion" — animations should snap.
bool macReduceMotion();

} // namespace locus
