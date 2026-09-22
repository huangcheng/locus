#include "platforms/macos/MacActivation.h"
#include "platforms/macos/MacLoginItem.h"
#include "platforms/macos/MacOverlay.h"

namespace locus {

void macActivateApplication() {}
void macStyleSettingsWindow(QWidget *) {}
bool macReduceMotion() { return false; }
void macMakeOverlayLiveWhenInactive(QWidget *, QWidget *) {}

bool macLaunchAtLoginEnabled() { return false; }
bool macSetLaunchAtLogin(bool) { return false; }

} // namespace locus
