#include "platform/MacActivation.h"

#import <AppKit/AppKit.h>

namespace navi {

void macActivateApplication() {
  NSApplication *app = [NSApplication sharedApplication];
  // Accessory = menubar agent: no Dock tile, no app menu bar. We still
  // activate on summon so the overlay window receives keyboard input.
  [app setActivationPolicy:NSApplicationActivationPolicyAccessory];
  [app activateIgnoringOtherApps:YES];
}

} // namespace navi
