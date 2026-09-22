#include "platform/MacActivation.h"

#include <QWidget>

#import <AppKit/AppKit.h>

namespace locus {

void macActivateApplication() {
  NSApplication *app = [NSApplication sharedApplication];
  // Accessory = menubar agent: no Dock tile, no app menu bar. We still
  // activate on summon so the overlay window receives keyboard input.
  [app setActivationPolicy:NSApplicationActivationPolicyAccessory];
  [app activateIgnoringOtherApps:YES];
}

void macStyleSettingsWindow(QWidget *window) {
  if (!window)
    return;
  window->winId(); // ensure the native NSWindow exists
  NSView *view = (__bridge NSView *)(void *)window->effectiveWinId();
  NSWindow *nsWindow = [view window];
  if (!nsWindow)
    return;
  nsWindow.titlebarAppearsTransparent = YES;
  nsWindow.titleVisibility = NSWindowTitleHidden;
  nsWindow.styleMask |= NSWindowStyleMaskFullSizeContentView;
  nsWindow.movableByWindowBackground = YES;
}

bool macReduceMotion() {
  if (@available(macOS 10.12, *))
    return NSWorkspace.sharedWorkspace.accessibilityDisplayShouldReduceMotion;
  return NO;
}

} // namespace locus
